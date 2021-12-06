#include "cache_server_task/cache_server_task.c"
#include "end_server_task/end_server_task.c"
#include "connect_task/connect_task.c"
#include "proxy.h"
#include "logger/log.h"
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>

int init_server_task(server_task* task){
    task->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(task->server_socket == -1){
        log_trace("THREAD %d: Not enough memory for allocating server socket", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    task->end_buf = malloc(sizeof(char) * PR_END_SERVER_BUFSIZ);
    if(!task->end_buf){
        close(task->server_socket);
        log_trace("THREAD %d: Not enough memory for allocating end server buffer", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    task->clients = malloc(sizeof(struct client_task*) * PR_SERVER_CLIENTS_INIT_CAP);
    if(!task->clients){
        close(task->server_socket);
        free(task->end_buf);
        log_trace("THREAD %d: Not enough memory for allocating server clients array", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    task->progress = 0;
    task->aborted = false;
    task->finished = false;
    task->query = NULL;
    return PR_SUCCESS;
}

int free_server_task(server_task* task){
    close(task->server_socket);
    free(task->end_buf);
    free(task->clients);
}
