#include "tasks/tasks.h"
#include "thread_pool/thread_pool.h"
#include "logger/log.h"

int set_end_client_task(abstract_task* task){
    task->task_func = do_end_client_task;
    task->abort_task = abort_client_task;
    task->type = END_CLIENT_TASK;
    return PR_SUCCESS;
}

int do_end_client_task(worker_thread* thread, abstract_task* task){
    client_task* dec_task = (client_task*)task;
    if(is_server_aborted(dec_task->server)){
        log_info("THREAD %d: Server finished. Socket: %d", curthread_id(), dec_task->client_socket);
        return task->abort_task(thread, task);
    }
    int to_read = dec_task->server->progress - dec_task->progress;
    if(to_read <= 0){
        log_trace("THREAD %d: Nothing to read yet. Waiting for server. Socket: %d", curthread_id(), dec_task->client_socket);
        remove_fd(thread, dec_task->client_socket);
#ifdef MULTITHREADED
      pthread_mutex_lock(&dec_task->server->clients_mutex);
#endif
        dec_task->server->end_clients_reading--;
        if(!dec_task->server->end_clients_reading){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&dec_task->server->clients_mutex);
#endif
            int add_val = add_server_task_fd(thread, task);
            if(add_val == PR_NOT_ENOUGH_MEMORY){
                log_trace("THREAD %d: Not enough memory to add server task. Socket: %d", curthread_id(), dec_task->client_socket);
                return task->abort_task(thread, task);
            }
        }
#ifdef MULTITHREADED
        else{
            pthread_mutex_unlock(&dec_task->server->clients_mutex);
        }
#endif
        return PR_CONTINUE;
    }
    int send_val = send(dec_task->client_socket, dec_task->server->end_buf + (dec_task->server->end_progress - to_read), to_read, MSG_NOSIGNAL);
    if(send_val == -1){
        if(errno == EWOULDBLOCK){
            return PR_CONTINUE;
        }
        else if(errno == EINTR){
            return PR_CONTINUE;
        }
        else{
            log_info("THREAD %d: Error occured while sending data. Socket : %d", curthread_id(), dec_task->client_socket);
            return task->abort_task(thread, task);
        }
    }
    else if(!send_val){
        log_info("THREAD %d: Client %d closed connection", curthread_id(), dec_task->client_socket);
        return task->abort_task(thread, task);
    }
    log_debug("THREAD %d: Send %d bytes to client. Socket: %d", curthread_id(), send_val, dec_task->client_socket);
    dec_task->progress += send_val;
    to_read -= send_val;
    if(!to_read){
        remove_fd(thread, dec_task->client_socket);
#ifdef MULTITHREADED
        pthread_mutex_lock(&dec_task->server->clients_mutex);
#endif
        dec_task->server->end_clients_reading--;
        if(!dec_task->server->end_clients_reading){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&dec_task->server->clients_mutex);
#endif
            int add_val = add_server_task_fd(thread, task);
            if(add_val == PR_NOT_ENOUGH_MEMORY){
                log_trace("THREAD %d: Not enough memory to add server task. Socket: %d", curthread_id(), dec_task->client_socket);
                return task->abort_task(thread, task);
            }
        }
#ifdef MULTITHREADED
        else{
            pthread_mutex_unlock(&dec_task->server->clients_mutex);
        }
#endif
    }
    return PR_CONTINUE;
}

int add_server_task_fd(worker_thread* thread, abstract_task* task){
    client_task* dec_task = (client_task*)task;
    worker_thread* opt = find_optimal_thread();
    int fd_val = add_fd(opt, dec_task->server->server_socket, POLLIN);
    if(fd_val == PR_NOT_ENOUGH_MEMORY){
        return PR_NOT_ENOUGH_MEMORY;
    }
}
