#include "tasks/tasks.h"
#include "cache/cache.h"
#include <unistd.h>

int init_client_task(client_task* task){
    task->last_exec = NULL;
    task->progress = 0;
    task->server = NULL;
    task->entry = NULL;
    return PR_SUCCESS;
}

int free_client_task(client_task* task){
    return PR_SUCCESS;
}

int abort_client_task(worker_thread* thread, abstract_task* task){
    client_task* dec_task = (client_task*)task;
#ifdef MULTITHREADED
    pthread_rwlock_rdlock(&gl_abort_lock);
#endif
    log_trace("THREAD %d: Aborting client task. Socket: %d", curthread_id(), dec_task->client_socket);
    if(dec_task->server)
        remove_client_task_from_server(dec_task->server, dec_task);
    if(dec_task->server){
#ifdef MULTITHREADED
      pthread_mutex_lock(&dec_task->server->clients_mutex);
#endif
        if(dec_task->server->clients_size == 0){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&dec_task->server->clients_mutex);
#endif
            if(is_server_aborted(dec_task->server)){
                log_trace("THREAD %d: I am last client of server %d. My socket: %d", curthread_id(), dec_task->server->server_socket, dec_task->client_socket);
                free(dec_task->server->clients);
                free(dec_task->server);
                cache_entry* aentry = find_entry_by_key(dec_task->url);
                if(aentry && !is_entry_finished(aentry)){
                    remove_entry_by_key(dec_task->url);
                }
            }
            else if(dec_task->server->type == END_SERVER_TASK){
                worker_thread* opt = find_optimal_thread();
                int fd_val = add_fd(opt, dec_task->server->server_socket, POLLIN);
            }
        }
#ifdef MULTITHREADED
        else{
            pthread_mutex_unlock(&dec_task->server->clients_mutex);
        }
#endif
    }
    free(dec_task->url);
    close(dec_task->client_socket);
    int ass_val = remove_assosiation_by_sock(dec_task->client_socket);
    assert(ass_val == PR_SUCCESS);
    int fd_val = remove_fd(thread, dec_task->client_socket);
    free(task);
#ifdef MULTITHREADED
    pthread_rwlock_unlock(&gl_abort_lock);
#endif
    if(errno == ENOMEM)
        return PR_NOT_ENOUGH_MEMORY;
    return PR_SUCCESS;
}

#include "cache_client_task/cache_client_task.c"
#include "end_client_task/end_client_task.c"
