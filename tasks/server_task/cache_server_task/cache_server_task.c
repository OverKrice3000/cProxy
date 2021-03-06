#include "tasks/tasks.h"
#include "thread_pool/thread_pool.h"
#include "logger/log.h"
#include "proxy.h"
#include "cache/cache.h"
#include "http_parser/parser.h"
#include <sys/types.h>
#include <sys/socket.h>

int set_cache_server_task(abstract_task* task){
    task->task_func = do_cache_server_task;
    task->abort_task = abort_server_task;
    task->type = CACHE_SERVER_TASK;
}

int do_cache_server_task(worker_thread* thread, abstract_task* task){
    server_task* dec_task = (server_task*)task;
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
#ifdef MULTITHREADED
    else{
        pthread_mutex_unlock(&dec_task->clients_mutex);
    }
#endif
    if(is_end_to_end()){
        log_trace("THREAD %d: Switching to end mode. Socket : %d", curthread_id(), dec_task->server_socket);
        int switch_val = server_switch_to_end_mode(thread, task);
        if(switch_val == PR_NOT_ENOUGH_MEMORY){
            log_trace("THREAD %d: Not enough memory to switch to end mode. Socket : %d", curthread_id(), dec_task->server_socket);
            return task->abort_task(thread, task);
        }
        return PR_CONTINUE;
    }
    ssize_t recv_val;
    if(!dec_task->entry)
        recv_val = recv(dec_task->server_socket, dec_task->end_buf + dec_task->end_progress, dec_task->end_capacity - dec_task->end_progress, 0);
    else
        recv_val = recv_entry_from_socket(dec_task->entry, dec_task->server_socket);
    if(recv_val == -1){
        if(errno == EWOULDBLOCK)
            return PR_CONTINUE;
        else if(errno == EINTR)
            return PR_CONTINUE;
        else{
            perror("");
            log_info("THREAD %d: Error occured while receiving data from socket %d", curthread_id(), dec_task->server_socket);
            return task->abort_task(thread, task);
        }
    }
    else if(recv_val == PR_NOT_ENOUGH_MEMORY){
        log_info("THREAD %d: Not enough memory to receive data from socket %d", curthread_id(), dec_task->server_socket);
        return task->abort_task(thread, task);
    }
    else if(!recv_val){
        log_info("THREAD %d: Finished reading from server. Socket: %d", curthread_id(), recv_val, dec_task->server_socket);
        set_entry_finished(dec_task->entry);
        return task->abort_task(thread, task);
    }
    log_debug("THREAD %d: Received %d bytes from server. Socket: %d", curthread_id(), recv_val, dec_task->server_socket);
    dec_task->end_progress += recv_val;
    dec_task->progress += recv_val;

    if(!dec_task->entry){
        if(dec_task->end_progress < 12){
            return PR_CONTINUE;
        }

        log_trace("THREAD %d: Received code from server: %.3s. Socket: %d", curthread_id(), dec_task->end_buf + 9, dec_task->server_socket);
        if(!starts_with_name("200\0", dec_task->end_buf + 9)){
            log_trace("THREAD %d: Switching to end mode. Socket : %d", curthread_id(), dec_task->server_socket);
            int switch_val = server_switch_to_end_mode(thread, task);
            if(switch_val == PR_NOT_ENOUGH_MEMORY){
                log_trace("THREAD %d: Not enough memory to switch to end mode. Socket : %d", curthread_id(), dec_task->server_socket);
                return task->abort_task(thread, task);
            }
            return PR_CONTINUE;
        }
        assert(!contains_entry(dec_task->url));
        cache_entry* addentr_val = add_entry(dec_task->url);
        if(!addentr_val){
            return PR_NOT_ENOUGH_MEMORY;
        }
        dec_task->entry = addentr_val;
        int append_val = append_entry(dec_task->entry, dec_task->end_buf, dec_task->end_progress);
        if(append_val == PR_NOT_ENOUGH_MEMORY){
            return PR_NOT_ENOUGH_MEMORY;
        }
    }
    dec_task->end_progress = 0;
    add_client_tasks_fd(thread, task);
    return PR_CONTINUE;
}

int server_switch_to_end_mode(worker_thread* thread, abstract_task* task){
    server_task* dec_task = (server_task*)task;
#ifdef MULTITHREADED
    pthread_mutex_lock(&dec_task->type_mutex);
    pthread_mutex_lock(&dec_task->clients_mutex);
#endif
    dec_task->end_clients_reading = dec_task->clients_size;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&dec_task->clients_mutex);
#endif
    int add_val = add_client_tasks_fd(thread, task);
    if(add_val == PR_NOT_ENOUGH_MEMORY){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&dec_task->type_mutex);
#endif
        return PR_NOT_ENOUGH_MEMORY;
    }
    remove_fd(thread, dec_task->server_socket);
    set_end_server_task(task);
#ifdef MULTITHREADED
    pthread_mutex_unlock(&dec_task->type_mutex);
#endif
    return PR_CONTINUE;
}



