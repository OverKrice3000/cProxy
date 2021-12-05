#include <stdio.h>
#include "proxy.h"
#include "tasks/tasks.h"
#include <sys/socket.h>
#include <sys/types.h>
#include "logger/log.h"
#include "socket_to_task/socket_to_task.h"
#include "clients/clients.h"
#include "thread_pool/thread_pool.h"
#include "errcodes.h"
#include "http_parser/parser.h"
#include <errno.h>
#include <assert.h>

int do_get_url_task(worker_thread* thread, abstract_task* task){
    get_url_task* dec_task = (get_url_task*)task;
    int recv_val = recv(dec_task->client_socket, dec_task->get_query + dec_task->progress, GET_MAX_LENGTH - dec_task->progress, 0);
    if(recv_val == -1){
        if(errno == ECONNRESET){
            log_info("THREAD %d: Client %d closed connection", curthread_id(), dec_task->client_id);
            return abort_get_url_task(thread, task);
        }
        else if(errno == ENOMEM){
            log_trace("THREAD %d: Not enough memory for resizing assosiations array", curthread_id());
            return PR_NOT_ENOUGH_MEMORY;
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
        log_info("THREAD %d: Client %d closed connection", curthread_id(), dec_task->client_id);
        return abort_get_url_task(thread, task);
    }
    dec_task->progress += recv_val;
    log_info("THREAD %d: GET_URL_TASK: Received %d bytes %d bytes total from client", curthread_id(), recv_val, dec_task->progress);
    int parse_val = parse_query(&dec_task->get_query, &dec_task->progress);
    if(parse_val == PR_METHOD_NOT_SUPPORTED){
        log_info("THREAD %d: Received an unsupported method from client", curthread_id(), dec_task->client_id);
        return abort_get_url_task(thread, task);
    }
    else if(parse_val == PR_VERSION_NOT_SUPPORTED){
        log_info("THREAD %d: Received an unsupported version from client", curthread_id(), dec_task->client_id);
        return abort_get_url_task(thread, task);
    }
    else if(parse_val == PR_QUERY_UNFINISHED){
        if(dec_task->progress == GET_MAX_LENGTH){
            log_info("THREAD %d: Received corrupted query from client %d", curthread_id(), dec_task->client_id);
            return abort_get_url_task(thread, task);
        }
        return PR_CONTINUE;
    }
    else if(parse_val == PR_SUCCESS){
        dec_task->get_query[dec_task->progress] = '\0';
        log_info("THREAD %d: Successfully parsed get query from client %d. Query :\n%s", curthread_id(), dec_task->client_id, dec_task->get_query);
        return abort_get_url_task(thread, task);
    }
}

int abort_get_url_task(worker_thread* thread, abstract_task* task){
    get_url_task* dec_task = (get_url_task*)task;
    int ass_val = remove_assosiation_by_sock(dec_task->client_socket);
    assert(ass_val == PR_SUCCESS);
    fprintf(stderr, "SOCKET %d CLIENT %d\n", dec_task->client_socket, dec_task->client_id);
    int client_val = remove_client_by_id(dec_task->client_id);
    assert(client_val == PR_SUCCESS);
    free(dec_task->get_query);
    int fd_val = remove_fd(thread, dec_task->client_socket);
    assert(fd_val == PR_SUCCESS);
    close(dec_task->client_socket);
    return PR_SUCCESS;
}

int init_get_url_task(get_url_task* task){
    task->task_func = do_get_url_task;
    task->abort_task = abort_get_url_task;
    task->type = GET_URL_TASK;
    task->progress = 0;
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
