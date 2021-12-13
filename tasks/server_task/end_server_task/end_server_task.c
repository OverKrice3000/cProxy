#include "tasks/tasks.h"
#include "thread_pool/thread_pool.h"
#include "logger/log.h"

int set_end_server_task(abstract_task* task){
    task->task_func = do_end_server_task;
    task->abort_task = abort_server_task;
    task->type = END_SERVER_TASK;
}

int do_end_server_task(worker_thread* thread, abstract_task* task){
    server_task* dec_task = (server_task*)task;
    dec_task->end_progress = 0;
#ifdef MULTITHREADED
    pthread_mutex_lock(&dec_task->clients_mutex);
#endif
    if(dec_task->clients_size == 0){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&dec_task->clients_mutex);
#endif
        log_info("THREAD %d: All clients of server %d aborted", curthread_id(), dec_task->server_socket);
        return task->abort_task(thread, task);
    }
    else{
        dec_task->end_clients_reading = dec_task->clients_size;
#ifdef MULTITHREADED
        pthread_mutex_unlock(&dec_task->clients_mutex);
#endif
    }
    ssize_t recv_val = recv(dec_task->server_socket, dec_task->end_buf, dec_task->end_capacity, 0);
    if(recv_val == -1){
        if(errno == EWOULDBLOCK)
            return PR_CONTINUE;
        else if(errno == EINTR)
            return PR_CONTINUE;
        else{
            log_info("THREAD %d: Error occured while receiving data from socket %d", curthread_id(), dec_task->server_socket);
            return task->abort_task(thread, task);
        }
    }
    else if(!recv_val){
        log_info("THREAD %d: Finished reading from server. Socket: %d", curthread_id(), dec_task->server_socket);
        return task->abort_task(thread, task);
    }
    log_debug("THREAD %d: Received %d bytes from server. Socket: %d", curthread_id(), recv_val, dec_task->server_socket);
    dec_task->end_progress += recv_val;
    dec_task->progress += recv_val;
    remove_fd(thread, dec_task->server_socket);
    int add_val = add_client_tasks_fd(thread, task);
    if(add_val == PR_NOT_ENOUGH_MEMORY){
        return task->abort_task(thread, task);
    }

    return PR_CONTINUE;
}
