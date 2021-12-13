#include <stdio.h>
#include "tasks/tasks.h"
#include "logger/log.h"
#include "thread_pool/thread_pool.h"

int set_connect_task(abstract_task* task){
    task->task_func = do_connect_task;
    task->abort_task = abort_server_task;
    task->type = CONNECT_TASK;
    return PR_SUCCESS;
}

int do_connect_task(worker_thread* thread, abstract_task* task){
    server_task* dec_task = (server_task*)task;
    ssize_t send_val = send(dec_task->server_socket, dec_task->query, dec_task->query_length - dec_task->progress, MSG_NOSIGNAL);
    if(send_val == -1){
        if(errno == EWOULDBLOCK)
            return PR_CONTINUE;
        else if(errno == EINTR)
            return PR_CONTINUE;
        else{
            log_info("THREAD %d: Error occured while sending data to socket %d", curthread_id(), dec_task->server_socket);
            return task->abort_task(thread, task);
        }
    }
    else if(!send_val){
        log_info("THREAD %d: Server closed connection. Socket: %d", curthread_id(), dec_task->server_socket);
        return task->abort_task(thread, task);
    }
    dec_task->progress += send_val;
    log_debug("THREAD %d: Sent %d query bytes to server. Socket : %d", curthread_id(), send_val, dec_task->server_socket);
    if(dec_task->progress != dec_task->query_length){
        return PR_CONTINUE;
    }
    dec_task->progress = 0;
#ifdef MULTITHREADED
    pthread_mutex_lock(&dec_task->clients_mutex);
#endif
    if(dec_task->clients_size == 0){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&dec_task->clients_mutex);
#endif
        log_info("THREAD %d: Found no clients on server with socket : %d", curthread_id(), dec_task->server_socket);
        return task->abort_task(thread, task);
    }
#ifdef MULTITHREADED
    pthread_mutex_lock(&dec_task->type_mutex);
#endif
    if(dec_task->clients[0]->type == CACHE_CLIENT_TASK)
        set_cache_server_task(task);
    else if(dec_task->clients[0]->type == END_CLIENT_TASK)
        set_end_server_task(task);
#ifdef MULTITHREADED
    pthread_mutex_unlock(&dec_task->type_mutex);
    pthread_mutex_unlock(&dec_task->clients_mutex);
#endif
    remove_fd(thread, dec_task->server_socket);
    int fd_val = add_fd(thread, dec_task->server_socket, POLLIN);
    if(fd_val == PR_NOT_ENOUGH_MEMORY){
        return task->abort_task(thread, task);
    }

    return PR_SUCCESS;
}

