#include <stdio.h>
#include <stdbool.h>
#include "proxy.h"
#include "tasks/tasks.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include "logger/log.h"
#include "cache/cache.h"
#include "socket_to_task/socket_to_task.h"
#include "thread_pool/thread_pool.h"
#include "errcodes.h"
#include "http_parser/parser.h"
#include <errno.h>
#include <assert.h>

#define NO_SERVER 0
#define SERVER_UP 1
#define SERVER_FINISHED 2

int do_get_url_task(worker_thread* thread, abstract_task* task){
    get_url_task* dec_task = (get_url_task*)task;
    int recv_val = recv(dec_task->client_socket, dec_task->get_query + dec_task->progress, GET_MAX_LENGTH - dec_task->progress, 0);
    if(recv_val == -1){
        if(errno == ECONNRESET){
            log_info("THREAD %d: Client with socket %d closed connection", curthread_id(), dec_task->client_socket);
            return abort_get_url_task(thread, task);
        }
        else if(errno == ENOMEM){
            log_trace("THREAD %d: Not enough memory for resizing assosiations array", curthread_id());
            return abort_get_url_task(thread, task);
        }
        else if(errno == EWOULDBLOCK){
            return PR_CONTINUE;
        }
        else if(errno == EINTR){
            return  PR_CONTINUE;
        }
        return abort_get_url_task(thread, task);
    }
    else if(recv_val == 0){
        log_info("THREAD %d: Client wtih socket %d closed connection", curthread_id(), dec_task->client_socket);
        return abort_get_url_task(thread, task);
    }
    dec_task->progress += recv_val;
    log_info("THREAD %d: GET_URL_TASK: Received %d bytes %d bytes total from client", curthread_id(), recv_val, dec_task->progress);
    int parse_val = parse_query(&dec_task->get_query, &dec_task->progress);
    if(parse_val == PR_METHOD_NOT_SUPPORTED){
        char* mna = "HTTP/1.0 501 Not Implemented\n\n\0";
        send(dec_task->client_socket, mna, strlen(mna), MSG_NOSIGNAL);
        log_info("THREAD %d: Received an unsupported method from client with socket %d", curthread_id(), dec_task->client_socket);
        return abort_get_url_task(thread, task);
    }
    else if(parse_val == PR_VERSION_NOT_SUPPORTED){
        log_info("THREAD %d: Received an unsupported version from client with socket %d", curthread_id(), dec_task->client_socket);
        return abort_get_url_task(thread, task);
    }
    else if(parse_val == PR_QUERY_UNFINISHED){
        if(dec_task->progress == GET_MAX_LENGTH){
            log_info("THREAD %d: Received corrupted query from client with socket %d", curthread_id(), dec_task->client_socket);
            return abort_get_url_task(thread, task);
        }
        return PR_CONTINUE;
    }
    dec_task->get_query[dec_task->progress] = '\0';
    set_conn_close(dec_task->get_query);
    log_info("THREAD %d: Successfully parsed get query from client with socket %d. Query :\n%s", curthread_id(), dec_task->client_socket, dec_task->get_query);
    int rm_ass_val = remove_assosiation_by_sock(dec_task->client_socket);
    assert(rm_ass_val == PR_SUCCESS);
    int rm_fd_val = remove_fd(thread, dec_task->client_socket);
    assert(rm_fd_val == PR_SUCCESS);
    dec_task->removed = true;

    int mode = NO_SERVER;
    client_task* client = malloc(sizeof(client_task));
    server_task* server = NULL;
    if(!client){
        log_trace("THREAD %d: Not enough memory for allocating client task for client with socket %d", curthread_id(), dec_task->client_socket);
        return abort_get_url_task(thread, task);
    }
    init_client_task(client);
    client->client_socket = dec_task->client_socket;
    int cpy_val = urlcpy(dec_task->get_query, &client->url);
    if(cpy_val == PR_NOT_ENOUGH_MEMORY){
        free(client);
        log_trace("THREAD %d: Not enough memory for allocating place for url for client with socket %d", curthread_id(), dec_task->client_socket);
        return abort_get_url_task(thread, task);
    }

#ifdef MULTITHREADED
    lock_assosiations();
#endif
    if(contains_finished_entry(client->url)){
        set_cache_client_task((abstract_task*)client);
        client->server = NULL;
        cache_entry* entry = find_entry_by_key(client->url);
        assert(entry);
        client->entry = entry;
        mode = SERVER_FINISHED;
    }
    else if(!is_end_to_end()){
        log_trace("THREAD %d: Adding new cache client. Socket : %d", curthread_id(), dec_task->client_socket);
        server_task* acc_server = NULL;
        for(int i = 0; i < task_assosiations.size; i++){
            abstract_task* next_task = task_assosiations.assosiations[i]->task;
            if(next_task->type == CACHE_CLIENT_TASK){
                client_task* dec_client_task = (client_task*)next_task;
                if(!strcmp(client->url, dec_client_task->url) && !dec_client_task->server->aborted){
                    assert(!acc_server || acc_server == dec_client_task->server);
                    assert(dec_client_task->server);
                    log_trace("THREAD %d: Found UP server task with the same url. Socket : %d. Url :\n%s", curthread_id(), dec_client_task->server->server_socket, client->url);
                    mode |= SERVER_UP;
                    acc_server = dec_client_task->server;
                }
            }
        }
        set_cache_client_task((abstract_task*)client);
        client->server = acc_server;
        if(acc_server)
            add_server_task_client(acc_server, client);
        if(mode == NO_SERVER){
            log_trace("THREAD %d: No server task found with the same url:\n%s", curthread_id(), client->url);
        }
    }
    else{
        log_trace("THREAD %d: Adding new end client. Socket : %d", curthread_id(), dec_task->client_socket);
        set_end_client_task((abstract_task*)client);
    }
#ifdef MULTITHREADED
    unlock_assosiations();
#endif

    if(mode == NO_SERVER){
        server = malloc(sizeof(server_task));
        if(!server){
            free(client);
            free(client->url);
            log_trace("THREAD %d: Not enough memory for allocating server task", curthread_id());
            return abort_get_url_task(thread, task);
        }
        set_connect_task((abstract_task*)server);
        int init_serv_val = init_server_task(server, dec_task->get_query);
        if(init_serv_val != PR_SUCCESS){
            free(client);
            free(client->url);
            free(server);
            log_trace("THREAD %d: Not enough memory for allocating server task", curthread_id());
            return abort_get_url_task(thread, task);
        }
        client->server = server;
        add_server_task_client(server, client);
    }

    int ass_val = add_assosiation(client->client_socket, (abstract_task*)client);
    if(ass_val == PR_NOT_ENOUGH_MEMORY){
        free(client);
        free(client->url);
        if(server){
            free_server_task(server);
            free(server);
        }
        log_trace("THREAD %d: Not enough memory for adding assosiation with client socket %d", curthread_id(), client->client_socket);
        return abort_get_url_task(thread, task);
    }

    if(mode == SERVER_FINISHED){
        worker_thread* opt_client = find_optimal_thread();
#ifdef MULTITHREADED
        pthread_mutex_lock(&opt_client->stop_mutex);
#endif
        int fd_val = add_fd(opt_client, client->client_socket, POLLOUT);
        if(fd_val == PR_NOT_ENOUGH_MEMORY){
            free(client);
            free(client->url);
            free_server_task(server);
            free(server);
            remove_assosiation_by_sock(client->client_socket);
#ifdef MULTITHREADED
            pthread_mutex_unlock(&opt_client->stop_mutex);
#endif
            log_trace("THREAD %d: Not enough memory for adding server fd %d", curthread_id(), server->server_socket);
            return abort_get_url_task(thread, task);
        }
#ifdef MULTITHREADED
        pthread_cond_signal(&opt_client->condvar);
        pthread_mutex_unlock(&opt_client->stop_mutex);
#endif
        free(dec_task->get_query);
    }
    else if(mode == NO_SERVER){
        worker_thread* opt_server = find_optimal_thread();
        ass_val = add_assosiation(server->server_socket, (abstract_task*)server);
        if(ass_val == PR_NOT_ENOUGH_MEMORY){
            free(client);
            free(client->url);
            free_server_task(server);
            free(server);
            remove_assosiation_by_sock(client->client_socket);
            log_trace("THREAD %d: Not enough memory for adding assosiation with server socket %d", curthread_id(), server->server_socket);
            return abort_get_url_task(thread, task);
        }
#ifdef MULTITHREADED
        pthread_mutex_lock(&opt_server->stop_mutex);
#endif
        int fd_val = add_fd(opt_server, server->server_socket, POLLOUT);
        if(fd_val == PR_NOT_ENOUGH_MEMORY){
            free(client);
            free(client->url);
            free_server_task(server);
            free(server);
            remove_assosiation_by_sock(client->client_socket);
            remove_assosiation_by_sock(server->server_socket);
#ifdef MULTITHREADED
            pthread_mutex_unlock(&opt_server->stop_mutex);
#endif
            log_trace("THREAD %d: Not enough memory for adding server fd %d", curthread_id(), server->server_socket);
            return abort_get_url_task(thread, task);
        }
#ifdef MULTITHREADED
        pthread_cond_signal(&opt_server->condvar);
        pthread_mutex_unlock(&opt_server->stop_mutex);
#endif
    }

    log_trace("THREAD %d: Successfully finished get_url_task", curthread_id());
    free(task);
    return PR_SUCCESS;
}

int abort_get_url_task(worker_thread* thread, abstract_task* task){
    get_url_task* dec_task = (get_url_task*)task;
    if(!dec_task->removed){
        int ass_val = remove_assosiation_by_sock(dec_task->client_socket);
        assert(ass_val == PR_SUCCESS);
        int fd_val = remove_fd(thread, dec_task->client_socket);
        assert(fd_val == PR_SUCCESS);
    }
    free(dec_task->get_query);
    close(dec_task->client_socket);
    free(task);
    if(errno == ENOMEM)
        return PR_NOT_ENOUGH_MEMORY;
    return PR_SUCCESS;
}

int init_get_url_task(get_url_task* task){
    task->task_func = do_get_url_task;
    task->abort_task = abort_get_url_task;
    task->type = GET_URL_TASK;
    task->progress = 0;
    task->removed = false;
    task->get_query = malloc(sizeof(char) * GET_MAX_LENGTH);
    if(!task->get_query){
        log_trace("THREAD %d: not enough memory to initialize get_url_task!", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    return PR_SUCCESS;
}

int free_get_url_task(get_url_task* task){
    free(task->get_query);
}
