#include <stdio.h>
#include "proxy.h"
#include <stdbool.h>
#include <stdlib.h>
#include "socket_to_task.h"
#include "errcodes.h"
#include "logger/log.h"
#include "thread_pool/thread_pool.h"
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

int init_assosiations(){
    task_assosiations.assosiations = malloc(sizeof(assosiation) * PR_ASSOSIATIONS_INIT_CAP);
    if(task_assosiations.assosiations){
        task_assosiations.capacity = PR_ASSOSIATIONS_INIT_CAP;
        task_assosiations.size = 0;
        return PR_SUCCESS;
    }
    else{
        task_assosiations.capacity = 0;
        task_assosiations.size = 0;
        return PR_NOT_ENOUGH_MEMORY;
    }
}

int add_assosiation(assosiation new_assosiation){
#ifdef MULTITHREADED
    pthread_mutex_lock(&task_assosiations.mutex);
#endif
    int return_code = PR_SUCCESS;
    if(task_assosiations.capacity == task_assosiations.size)
        return_code = resize_assosiations();
    if(return_code == PR_NOT_ENOUGH_MEMORY){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&task_assosiations.mutex);
#endif
        log_trace("THREAD %d: Could not add assosiation with socket : %d", curthread_id(), new_assosiation.socket);
        return PR_NOT_ENOUGH_MEMORY;
    }
    task_assosiations.assosiations[task_assosiations.size++] = new_assosiation;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&task_assosiations.mutex);
#endif
    log_trace("THREAD %d: Successfully added assosiation with socket : %d", curthread_id(), new_assosiation.socket);
    return PR_SUCCESS;
}

int remove_assosiation_by_sock(int sock){
#ifdef MULTITHREADED
    pthread_mutex_lock(&task_assosiations.mutex);
#endif
    bool removed = false;
    for(int i = 0; i < task_assosiations.size; i++){
        if(task_assosiations.assosiations[i].socket == sock){
            task_assosiations.assosiations[i] = task_assosiations.assosiations[--task_assosiations.size];
            removed = true;
            break;
        }
    }
#ifdef MULTITHREADED
    pthread_mutex_unlock(&task_assosiations.mutex);
#endif
    (removed) ? log_trace("THREAD %d: Successfully removed assosiation with socket : %d", curthread_id(), sock) : log_trace("THREAD %d: No assosiation with such socket : %d", curthread_id(), sock);
    return (removed) ? PR_SUCCESS : PR_NO_TASK_ASSOSIATED;
}

assosiation* find_assosiation_by_sock(int sock){
#ifdef MULTITHREADED
    pthread_mutex_lock(&task_assosiations.mutex);
#endif
    for(int i = 0; i < task_assosiations.size; i++){
        if(task_assosiations.assosiations[i].socket == sock){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&task_assosiations.mutex);
#endif
            log_trace("THREAD %d: Found assosiation with such socket : %d", curthread_id(), sock);
            return task_assosiations.assosiations + i;
        }
    }
#ifdef MULTITHREADED
    pthread_mutex_unlock(&task_assosiations.mutex);
#endif
    log_trace("THREAD %d: No assosiation with such socket : %d", curthread_id(), sock);
    return NULL;
}

int resize_assosiations(){
    assosiation* new_ptr = realloc(task_assosiations.assosiations, sizeof(assosiation) * task_assosiations.capacity * 2);
    if(new_ptr) {
        task_assosiations.assosiations = new_ptr;
        task_assosiations.capacity *= 2;
        log_trace("THREAD %d: Successfully resized assosiations array", curthread_id());
        return PR_SUCCESS;
    }
    else{
        log_trace("THREAD %d: Not enough memory for resizing assosiations array", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
}

int destroy_assosiations(){
    free(task_assosiations.assosiations);
    return PR_SUCCESS;
}

#ifdef MULTITHREADED
void lock_assosiations(){
    pthread_mutex_lock(&task_assosiations.mutex);
}

void unlock_assosiations(){
    pthread_mutex_unlock(&task_assosiations.mutex);
}
#endif
