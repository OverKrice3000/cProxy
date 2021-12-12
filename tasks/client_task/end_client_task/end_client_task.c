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
#ifdef MULTITHREADED
    pthread_mutex_lock(&temp_mutex);
#endif
    client_task* dec_task = (client_task*)task;
    if(is_server_aborted(dec_task->server)){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&temp_mutex);
#endif
        log_info("THREAD %d: Server finished. Socket: %d", curthread_id(), dec_task->client_socket);
        return task->abort_task(thread, task);
    }
    int to_read = dec_task->server->progress - dec_task->progress;
    assert(to_read > 0);
    int send_val = send(dec_task->client_socket, dec_task->server->end_buf + (dec_task->server->end_progress - to_read), to_read, MSG_NOSIGNAL);
    if(send_val == -1){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&temp_mutex);
#endif
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
#ifdef MULTITHREADED
        pthread_mutex_unlock(&temp_mutex);
#endif
        log_info("THREAD %d: Client %d closed connection", curthread_id(), dec_task->client_socket);
        return task->abort_task(thread, task);
    }
    log_debug("THREAD %d: Send %d bytes to client. Socket: %d", curthread_id(), send_val, dec_task->client_socket);
    dec_task->progress += send_val;
    to_read -= send_val;
    if(!to_read){
        dec_task->server->end_clients_reading--;
        if(!dec_task->server->end_clients_reading){
            worker_thread* opt = find_optimal_thread();
#ifdef MULTITHREADED
            pthread_mutex_lock(&opt->stop_mutex);
#endif
            int fd_val = add_fd(opt, dec_task->server->server_socket, POLLIN);
            if(fd_val == PR_NOT_ENOUGH_MEMORY){
#ifdef MULTITHREADED
                pthread_mutex_unlock(&temp_mutex);
                pthread_mutex_unlock(&opt->stop_mutex);
#endif
                return task->abort_task(thread, task);
            }
#ifdef MULTITHREADED
            pthread_cond_signal(&opt->condvar);
            pthread_mutex_unlock(&opt->stop_mutex);
#endif
        }
        remove_fd(thread, dec_task->client_socket);
    }
#ifdef MULTITHREADED
    pthread_mutex_unlock(&temp_mutex);
#endif
    return PR_CONTINUE;
}

