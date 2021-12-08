#include "cache_server_task/cache_server_task.c"
#include "end_server_task/end_server_task.c"
#include "connect_task/connect_task.c"
#include "tasks/tasks.h"
#include "http_parser/parser.h"
#include "proxy.h"
#include "logger/log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

int init_server_task(server_task* task, char* query){
    task->progress = 0;
    task->aborted = false;
    task->query = query;
    task->query_length = strlen(query);
    task->entry = NULL;
    task->end_progress = 0;
    task->end_clients_reading = 0;
    task->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(task->server_socket == -1){
        log_trace("THREAD %d: Not enough memory for allocating server task socket", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    fcntl(task->server_socket, F_SETFL, fcntl(task->server_socket, F_GETFL) | O_NONBLOCK);
    int true_val = 1;
    if(setsockopt(task->server_socket, SOL_SOCKET, SO_REUSEADDR, &true_val, sizeof(int))){
        close(task->server_socket);
        log_trace("THREAD %d: Could not set socket option", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    struct sockaddr_in addr;
    addr.sin_port = htons(0);
    addr.sin_family = AF_INET;
    memset(addr.sin_zero, 0, 8);
    int aton_val = inet_aton("10.4.0.68", &addr.sin_addr);
    assert(aton_val);
    if(bind(task->server_socket, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))){
        close(task->server_socket);
        log_trace("THREAD %d: Could not bind server task socket", curthread_id());
        return PR_COULD_NOT_CONNECT;
    }

    char* host_val;
    int host_length;
    int find_header_val = find_header_by_name(query, "Host\0", &host_val, &host_length);
    if(find_header_val == PR_NO_SUCH_HEADER){
        close(task->server_socket);
        log_trace("THREAD %d: Could not find host header", curthread_id());
        return PR_NO_SUCH_HEADER;
    }
    char savsymb = host_val[host_length];
    host_val[host_length] = '\0';

    struct hostent hostent;
    char* gethostbuf = malloc(sizeof(char) * PR_GETHOST_BUFSIZ);
    if(!gethostbuf){
        close(task->server_socket);
        log_trace("THREAD %d: Could not allocate buffer for host resolving", curthread_id());
        host_val[host_length] = savsymb;
        return PR_NOT_ENOUGH_MEMORY;
    }
    int gethosterr;
    if(!gethostbyname_r(host_val, &hostent, gethostbuf, PR_GETHOST_BUFSIZ, &gethosterr) || !hostent.h_length){
        close(task->server_socket);
        free(gethostbuf);
        log_trace("THREAD %d: Could not resolve host with error %d:\n%s", curthread_id(), gethosterr, host_val);
        host_val[host_length] = savsymb;
        return PR_NO_SUCH_HOST;
    }
    host_val[host_length] = savsymb;

    if(hostent.h_addrtype != AF_INET){
        close(task->server_socket);
        free(gethostbuf);
        log_trace("THREAD %d: Unsupported host address type %d for:\n%s", curthread_id(), hostent.h_addrtype, host_val);
        return PR_HOST_UNSUPPORTED_AF;
    }

    struct sockaddr_in host_addr;
    host_addr.sin_port = htons(80);
    host_addr.sin_family = AF_INET;
    memset(host_addr.sin_zero, 0, 8);
    memcpy(&host_addr.sin_addr.s_addr, hostent.h_addr_list[0], sizeof(host_addr.sin_addr.s_addr));
    log_trace("THREAD %d: Parsed address %s", curthread_id(), inet_ntoa(host_addr.sin_addr));
    int connect_val = connect(task->server_socket, (struct sockaddr*)&host_addr, sizeof(struct sockaddr_in));
    if(connect_val && errno != EINPROGRESS){
        close(task->server_socket);
        free(gethostbuf);
        log_trace("THREAD %d: Could not connect to host:\n%s", curthread_id(), host_val);
        return PR_COULD_NOT_CONNECT;
    }
    free(gethostbuf);
    task->end_buf = malloc(sizeof(char) * PR_END_SERVER_BUFSIZ);
    if(!task->end_buf){
        close(task->server_socket);
        log_trace("THREAD %d: Not enough memory for allocating end server buffer", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    task->end_capacity = PR_END_SERVER_BUFSIZ;
    task->clients = malloc(sizeof(struct client_task*) * PR_SERVER_CLIENTS_INIT_CAP);
    if(!task->clients){
        close(task->server_socket);
        free(task->end_buf);
        log_trace("THREAD %d: Not enough memory for allocating server clients array", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    task->clients_size = 0;
    task->clients_capacity = PR_SERVER_CLIENTS_INIT_CAP;
    int cpy_val = urlcpy(query, &task->url);
    if(cpy_val == PR_NOT_ENOUGH_MEMORY){
        close(task->server_socket);
        free(task->end_buf);
        free(task->clients);
        log_trace("THREAD %d: Not enough memory for allocating place for url", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
#ifdef MULTITHREADED
    if(pthread_mutex_init(&task->clients_mutex, NULL)){
        close(task->server_socket);
        free(task->end_buf);
        free(task->url);
        free(task->clients);
        log_trace("THREAD %d: Not enough memory for allocating server clients array", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
#endif
    return PR_SUCCESS;
}

int free_server_task(server_task* task){
    close(task->server_socket);
    free(task->end_buf);
    free(task->url);
    free(task->clients);
#ifdef MULTITHREADED
    pthread_mutex_destroy(&task->clients_mutex);
#endif
}

int add_server_task_client(server_task* server, struct client_task* client){
#ifdef MULTITHREADED
    pthread_mutex_lock(&server->clients_mutex);
#endif
    int return_code = PR_SUCCESS;
    if(server->clients_capacity == server->clients_size)
        return_code = resize_server_task_clients(server);
    if(return_code == PR_NOT_ENOUGH_MEMORY){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&server->clients_mutex);
#endif
        log_trace("THREAD %d: Could not add new client task with socket %d to server task with socket %d", curthread_id(), client->client_socket, server->server_socket);
        return PR_NOT_ENOUGH_MEMORY;
    }
    server->clients[server->clients_size++] = client;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&server->clients_mutex);
#endif
    log_trace("THREAD %d: Successfully added new client task with socket %d to server task with socket %d", curthread_id(), client->client_socket, server->server_socket);
    return PR_SUCCESS;
}

int resize_server_task_clients(server_task* server){
    struct client_task** new_ptr = realloc(server->clients, sizeof(struct client_task*) * server->clients_capacity * 2);
    if(!new_ptr) {
        log_trace("THREAD %d: Not enough memory for resizing clients array for server task with socket %d", curthread_id(), server->server_socket);
        return PR_NOT_ENOUGH_MEMORY;
    }
    server->clients = new_ptr;
    server->clients_capacity *= 2;
    log_trace("THREAD %d: Successfully resized clients array for server task with socket %d", curthread_id(), server->server_socket);
    return PR_SUCCESS;
}

int abort_server_task(worker_thread* thread, abstract_task* task){
    server_task* dec_task = (server_task*)task;
    log_trace("THREAD %d: Aborting server task. Socket: %d", curthread_id(), dec_task->server_socket);
    set_server_aborted(dec_task);
    if(dec_task->clients_size == 0){
        log_trace("THREAD %d: Did not find any clients while aborting server task. Socket: %d", curthread_id(), dec_task->server_socket);
        free(dec_task->clients);
        cache_entry* aentry = find_entry_by_key(dec_task->url);
        if(aentry && !is_entry_finished(aentry)){
            remove_entry_by_key(dec_task->url);
        }
    }
    else{
        add_client_tasks_fd(thread, task);
    }
    close(dec_task->server_socket);
    free(dec_task->end_buf);
    free(dec_task->query);
    free(dec_task->url);
    int ass_val = remove_assosiation_by_sock(dec_task->server_socket);
    assert(ass_val == PR_SUCCESS);
    int fd_val = remove_fd(thread, dec_task->server_socket);
    assert(fd_val == PR_SUCCESS);
    if(dec_task->clients_size == 0)
        free(task);
    return PR_SUCCESS;
}

int add_client_tasks_fd(worker_thread* thread, abstract_task* task){
    server_task* dec_task = (server_task*)task;
    log_trace("THREAD %d: Server adding clients fds to threads. Socket: %d", curthread_id(), dec_task->server_socket);
    for(int i = 0; i < dec_task->clients_size; i++){
        if(dec_task->clients[i]->last_exec && contains_fd(dec_task->clients[i]->last_exec, dec_task->clients[i]->client_socket))
            continue;
        worker_thread* opt = find_optimal_thread();
        int fd_val = add_fd(opt, dec_task->clients[i]->client_socket, POLLOUT);
        dec_task->clients[i]->last_exec = opt;
        if(fd_val == PR_NOT_ENOUGH_MEMORY)
            return PR_NOT_ENOUGH_MEMORY;
    }
    return PR_SUCCESS;
}

bool is_server_aborted(server_task* task){
    return task->aborted;
}

void set_server_aborted(server_task* task){
   task->aborted = true;
}

