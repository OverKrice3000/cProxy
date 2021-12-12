#include "tasks/tasks.h"
#include <stdbool.h>
#include "proxy.h"
#include "thread_pool/thread_pool.h"
#include "cache/cache.h"
#include "logger/log.h"

int set_cache_client_task(abstract_task* task){
    task->task_func = do_cache_client_task;
    task->abort_task = abort_client_task;
    task->type = CACHE_CLIENT_TASK;
}

int do_cache_client_task(worker_thread* thread, abstract_task* task){
#ifdef MULTITHREADED
    pthread_mutex_lock(&temp_mutex);
#endif
    client_task* dec_task = (client_task*)task;
    if(!dec_task->entry){
        if(dec_task->server->entry){
            dec_task->entry = dec_task->server->entry;
        }
        else if(dec_task->server->type == END_SERVER_TASK){
            log_trace("THREAD %d: Server changed to end mode. Socket : %d", curthread_id(), dec_task->client_socket);
            client_switch_to_end_mode(thread, task);
#ifdef MULTITHREADED
            pthread_mutex_unlock(&temp_mutex);
#endif
            return PR_CONTINUE;
        }
        else if(is_server_aborted(dec_task->server)){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&temp_mutex);
#endif
            log_info("THREAD %d: Server aborted connection. Socket : %d", curthread_id(), dec_task->client_socket);
            return task->abort_task(thread, task);
        }
        else{
            log_trace("THREAD %d: No entry found. Waiting for server. Socket : %d", curthread_id(), dec_task->client_socket);
            remove_fd(thread, dec_task->client_socket);
#ifdef MULTITHREADED
            pthread_mutex_unlock(&temp_mutex);
#endif
            return PR_CONTINUE;
        }
    }
    int send_val = send_entry_to_socket(dec_task->entry, dec_task->client_socket, dec_task->progress);
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
    else if(send_val == PR_ENTRY_NO_NEW_DATA){
        if(is_entry_finished(dec_task->entry) == true){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&temp_mutex);
#endif
            log_info("THREAD %d: Finished sending data to client %d", curthread_id(), dec_task->client_socket);
            return task->abort_task(thread, task);
        }
        else if(is_server_aborted(dec_task->server) == true){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&temp_mutex);
#endif
            log_info("THREAD %d: Server aborted connection. Socket : %d", curthread_id(), dec_task->client_socket);
            return task->abort_task(thread, task);
        }
        else if(dec_task->server->type == END_SERVER_TASK){
            log_trace("THREAD %d: Server changed to end mode. Socket : %d", curthread_id(), dec_task->client_socket);
            client_switch_to_end_mode(thread, task);
#ifdef MULTITHREADED
            pthread_mutex_unlock(&temp_mutex);
#endif
            return PR_CONTINUE;
        }
        else{
            log_trace("THREAD %d: No new data is availible at the moment. Socket : %d", curthread_id(), dec_task->client_socket);
            remove_fd(thread, dec_task->client_socket);
#ifdef MULTITHREADED
            pthread_mutex_unlock(&temp_mutex);
#endif
            return PR_CONTINUE;
        }
    }
    dec_task->progress += send_val;
    log_debug("THREAD %d: Send %d bytes to client. Socket: %d", curthread_id(), send_val, dec_task->client_socket);
#ifdef MULTITHREADED
    pthread_mutex_unlock(&temp_mutex);
#endif
    return PR_CONTINUE;
}


int client_switch_to_end_mode(worker_thread* thread, abstract_task* task){
    set_end_client_task(task);
    return PR_SUCCESS;
}
