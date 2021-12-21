#include <stdio.h>
#include "proxy.h"
#include <stdlib.h>
#include <stdbool.h>
#include "errcodes.h"
#include <assert.h>
#include <errno.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif
#include "logger/log.h"
#include <poll.h>
#include "thread_pool.h"
#include <sys/resource.h>
#include "tasks/tasks.h"
#include <unistd.h>
#include <signal.h>
#include "socket_to_task/socket_to_task.h"

void* worker_thread_func(void* arg){
    worker_thread* self = (worker_thread*)arg;
    log_trace("THREAD %d: Starting cycle!", curthread_id());
    while(true){
        log_trace("THREAD %d: Starting iteration!", curthread_id());
#ifdef MULTITHREADED
        pthread_mutex_lock(&self->stop_mutex);
        pthread_mutex_lock(&self->nsocks_mutex);
        while(self->nsocks <= 0){
            log_info("THREAD %d: No tasks found. I sleep.", curthread_id());
            pthread_mutex_unlock(&self->nsocks_mutex);
            pthread_cond_wait(&self->condvar, &self->stop_mutex);
            if(is_finished()){
                pthread_mutex_unlock(&self->stop_mutex);
                break;
            }
            pthread_mutex_lock(&self->nsocks_mutex);
        }
        if(is_finished()){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&self->nsocks_mutex);
#endif
            break;
        }

#endif
        int iter_nsocks = self->nsocks;
#ifdef MULTITHREADED
        pthread_mutex_unlock(&self->nsocks_mutex);
        pthread_mutex_unlock(&self->stop_mutex);
        pthread_mutex_lock(&self->poll_mutex);
#endif
        log_info("THREAD %d: New iteration has %d fds", curthread_id(), iter_nsocks);
        int poll_val = poll(self->socks, iter_nsocks, -1);
        log_trace("THREAD %d: 1AHAHA1", curthread_id(), iter_nsocks);
        if(is_finished()){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&self->poll_mutex);
#endif
            break;
        }

#ifdef MULTITHREADED
        pthread_mutex_unlock(&self->poll_mutex);
#endif
        if(poll_val == -1){
            log_warn("THREAD %d: Poll error occured!", curthread_id());
            continue;
        }
        for(int i = iter_nsocks - 1; i >= 0; i--){
            log_trace("THREAD %d: 2AHAHA2", curthread_id(), iter_nsocks);
#ifdef MULTITHREADED
            pthread_mutex_lock(&self->nsocks_mutex);
#endif
            struct pollfd cur_sock = self->socks[i];

#ifdef MULTITHREADED
            pthread_mutex_unlock(&self->nsocks_mutex);
#endif
            if(cur_sock.revents != 0){
                log_trace("THREAD %d: 3AHAHA3", curthread_id(), iter_nsocks);
                assosiation* my_ass = find_assosiation_by_sock(cur_sock.fd);
                log_trace("THREAD %d: 4AHAHA4", curthread_id(), iter_nsocks);
                abstract_task* this_task = my_ass->task;
                log_trace("THREAD %d: Processing next socket %d i %d task type %d", curthread_id(), cur_sock.fd, i, this_task->type);
                if(cur_sock.revents & POLLERR){
                    log_warn("THREAD %d: REVENTS %d POLLERR %d", curthread_id(), cur_sock.revents, POLLERR);
                    log_warn("THREAD %d: Socket %d has an error pending! Aborting assosiated task!", curthread_id(), cur_sock.fd);
                    this_task->abort_task(self, this_task);
                }
                else{
                    int task_val = this_task->task_func(self, this_task);
                    if(task_val == PR_NOT_ENOUGH_MEMORY){
                        log_debug("THREAD %d: Not enough memory! Switching to end-to-end mode", curthread_id());
                        set_end_to_end();
                    }
                }
                cur_sock.revents = 0;
                if(is_finished())
                    break;
            }
        }
        if(is_finished())
            break;
    }
#ifdef MULTITHREADED
    log_info("THREAD %d: Finished! Signalling other threads!", curthread_id());
    for(int i = 0; i < pool.size; i++){
        pthread_cond_signal(&pool.threads[i].condvar);
    }
#endif
    return NULL;
}

int init_thread_pool(int capacity){
    pool.threads = malloc(sizeof(worker_thread) * capacity);
    if(!pool.threads){
        pool.capacity = 0;
        pool.size = 0;
        return PR_NOT_ENOUGH_MEMORY;
    }
    pool.capacity = capacity;
    pool.size = 0;
    return PR_SUCCESS;
}

#ifdef MULTITHREADED
int start_worker_thread(){
    assert(pool.capacity != 0);
    assert(pool.capacity != pool.size);
    worker_thread* new_thread = pool.threads + pool.size;
    new_thread->nsocks = 0;
    new_thread->socks = malloc(sizeof(struct pollfd) * PR_POLLFD_INIT_CAPACITY);
    if(!new_thread->socks){
        new_thread->pollfd_capacity = 0;
        return PR_NOT_ENOUGH_MEMORY;
    }
    new_thread->pollfd_capacity = PR_POLLFD_INIT_CAPACITY;
    if(pthread_cond_init(&new_thread->condvar, NULL)){
        free(new_thread->socks);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_mutex_init(&new_thread->poll_mutex, NULL)){
        free(new_thread->socks);
        pthread_cond_destroy(&new_thread->condvar);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_mutex_init(&new_thread->nsocks_mutex, NULL)){
        free(new_thread->socks);
        pthread_cond_destroy(&new_thread->condvar);
        pthread_mutex_destroy(&new_thread->poll_mutex);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_mutex_init(&new_thread->stop_mutex, NULL)){
        free(new_thread->socks);
        pthread_cond_destroy(&new_thread->condvar);
        pthread_mutex_destroy(&new_thread->poll_mutex);
        pthread_mutex_destroy(&new_thread->nsocks_mutex);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_create(&new_thread->id, NULL, worker_thread_func, (void*)new_thread)){
        free(new_thread->socks);
        pthread_cond_destroy(&new_thread->condvar);
        pthread_mutex_destroy(&new_thread->poll_mutex);
        pthread_mutex_destroy(&new_thread->nsocks_mutex);
        pthread_mutex_destroy(&new_thread->stop_mutex);
        return PR_COULD_NOT_START_THREAD;
    }
    pool.size++;
    return PR_SUCCESS;
}
#endif

int add_curthread(){
    assert(pool.capacity != 0);
    assert(pool.capacity != pool.size);
    worker_thread* new_thread = pool.threads + pool.size;
    new_thread->nsocks = 0;
    new_thread->socks = malloc(sizeof(struct pollfd) * PR_POLLFD_INIT_CAPACITY);
    if(!new_thread->socks){
        new_thread->pollfd_capacity = 0;
        return PR_NOT_ENOUGH_MEMORY;
    }
    new_thread->pollfd_capacity = PR_POLLFD_INIT_CAPACITY;
    new_thread->id = 0;
#ifdef MULTITHREADED
    if(pthread_cond_init(&new_thread->condvar, NULL)){
        free(new_thread->socks);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_mutex_init(&new_thread->poll_mutex, NULL)){
        free(new_thread->socks);
        pthread_cond_destroy(&new_thread->condvar);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_mutex_init(&new_thread->nsocks_mutex, NULL)){
        free(new_thread->socks);
        pthread_cond_destroy(&new_thread->condvar);
        pthread_mutex_destroy(&new_thread->poll_mutex);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_mutex_init(&new_thread->stop_mutex, NULL)){
        free(new_thread->socks);
        pthread_cond_destroy(&new_thread->condvar);
        pthread_mutex_destroy(&new_thread->poll_mutex);
        pthread_mutex_destroy(&new_thread->nsocks_mutex);
        return PR_NOT_ENOUGH_MEMORY;
    }
    new_thread->id = pthread_self();
#endif
    pool.size++;
    return PR_SUCCESS;
}

int close_worker_threads(){
#ifdef MULTITHREADED
    for(int i = 0; i < pool.capacity - 1 && i < pool.size; i++){
        int cancel_val = pthread_cancel(pool.threads[i].id);
        assert(!cancel_val);
    }
#endif
    return PR_SUCCESS;
}

int join_worker_threads(){
#ifdef MULTITHREADED
    for(int i = 0; i < pool.capacity - 1 && i < pool.size; i++){
        int join_val = pthread_join(pool.threads[i].id, NULL);
        assert(!join_val);
    }
#endif
    return PR_SUCCESS;
}

int destroy_thread_pool(){
    for(int i = 0; i < pool.size; i++){
#ifdef MULTITHREADED
        pthread_cond_destroy(&pool.threads[i].condvar);
        pthread_mutex_destroy(&pool.threads[i].poll_mutex);
        pthread_mutex_destroy(&pool.threads[i].nsocks_mutex);
        pthread_mutex_destroy(&pool.threads[i].stop_mutex);
#endif
        free(pool.threads[i].socks);
    }
    free(pool.threads);
}

worker_thread* find_optimal_thread(){
    assert(pool.size == pool.capacity);
    worker_thread* optimal_thread = pool.threads;

#ifdef MULTITHREADED
    pthread_mutex_lock(&optimal_thread->nsocks_mutex);
    pthread_mutex_unlock(&optimal_thread->nsocks_mutex);
    for(int i = 1; i < pool.size; i++){
        pthread_mutex_lock(&pool.threads[i].nsocks_mutex);
        pthread_mutex_lock(&optimal_thread->nsocks_mutex);
        if(pool.threads[i].nsocks < optimal_thread->nsocks){
            pthread_mutex_unlock(&optimal_thread->nsocks_mutex);
            optimal_thread = pool.threads + i;
        }
        else{
            pthread_mutex_unlock(&optimal_thread->nsocks_mutex);
        }
        pthread_mutex_unlock(&pool.threads[i].nsocks_mutex);
    }
#endif
    log_warn("THREAD %d: Current optimal thread has id %d", curthread_id(), optimal_thread->id);
    return optimal_thread;
}

worker_thread_t curthread_id(){
#ifndef MULTITHREADED
    return 0;
#else
    return pthread_self();
#endif
}

int add_fd(worker_thread* thread, int fd, short events){
#ifdef MULTITHREADED
    pthread_mutex_lock(&thread->stop_mutex);
    pthread_mutex_lock(&thread->nsocks_mutex);
#endif
    int return_code = PR_SUCCESS;
    if(thread->pollfd_capacity == thread->nsocks){
        log_trace("THREAD %d: Resizing fds array of %d thread", curthread_id(), thread->id);
#ifdef MULTITHREADED
        pthread_mutex_lock(&thread->poll_mutex);
#endif
        return_code = resize_fds(thread);
#ifdef MULTITHREADED
        pthread_mutex_unlock(&thread->poll_mutex);
#endif
    }

    if(return_code == PR_NOT_ENOUGH_MEMORY){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&thread->nsocks_mutex);
        pthread_mutex_unlock(&thread->stop_mutex);
#endif
        log_trace("THREAD %d: Could not add fd %d to thread %d", curthread_id(), fd, thread->id);
        return PR_NOT_ENOUGH_MEMORY;
    }
    struct pollfd new_fd = {
            .fd = fd,
            .events = events,
            .revents = 0
    };
    thread->socks[thread->nsocks++] = new_fd;
#ifdef MULTITHREADED
    pthread_kill(thread->id, SIGUSR1);
    pthread_cond_signal(&thread->condvar);
    pthread_mutex_unlock(&thread->nsocks_mutex);
    pthread_mutex_unlock(&thread->stop_mutex);
#endif
    log_trace("THREAD %d: Successfully added fd %d to thread %d", curthread_id(), fd, thread->id);
    return PR_SUCCESS;
}

int remove_fd(worker_thread* thread, int fd){
    int index = find_fd_index(thread, fd);
#ifdef MULTITHREADED
    pthread_mutex_lock(&thread->nsocks_mutex);
#endif
    assert(index < thread->nsocks && index >= 0);
    if(index == PR_NO_SUCH_FD_IN_THREAD) {
        log_trace("THREAD %d: No such fd %d in thread %d", curthread_id(), fd, thread->id);
        return PR_NO_SUCH_FD_IN_THREAD;
    }

    thread->socks[index] = thread->socks[--thread->nsocks];
#ifdef MULTITHREADED
    pthread_mutex_unlock(&thread->nsocks_mutex);
#endif
    log_trace("THREAD %d: Successfully removed fd %d from thread %d", curthread_id(), fd, thread->id);
    return PR_SUCCESS;
}

int resize_fds(worker_thread* thread){
    assert(thread->pollfd_capacity == thread->nsocks);
    struct pollfd* new_ptr = realloc(thread->socks, sizeof(struct pollfd) * thread->pollfd_capacity * 2);
    if(new_ptr) {
        thread->socks = new_ptr;
        thread->pollfd_capacity *= 2;
        log_trace("Successfully resized thread pollfd array");
        return PR_SUCCESS;
    }
    else{
        log_trace("Not enough memory for resizing thread pollfd array");
        return PR_NOT_ENOUGH_MEMORY;
    }
}

int find_fd_index(worker_thread* thread, int fd){
#ifdef MULTITHREADED
    pthread_mutex_lock(&thread->nsocks_mutex);
#endif
    for(int i = 0; i < thread->nsocks; i++){
        if(thread->socks[i].fd == fd){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&thread->nsocks_mutex);
#endif
            log_trace("THREAD %d: Successfully found fd %d from thread %d", curthread_id(), fd, thread->id);
            return i;
        }
    }
#ifdef MULTITHREADED
    pthread_mutex_unlock(&thread->nsocks_mutex);
#endif
    log_trace("THREAD %d: No such fd %d in thread %d", curthread_id(), fd, thread->id);
    return PR_NO_SUCH_FD_IN_THREAD;
}

bool contains_fd(worker_thread* thread, int fd){
    return find_fd_index(thread, fd) != PR_NO_SUCH_FD_IN_THREAD;
}

