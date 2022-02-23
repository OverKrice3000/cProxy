#ifndef F_THREAD_POOL
#define F_THREAD_POOL

#include <stdio.h>
#include <stdbool.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

#ifdef MULTITHREADED
typedef pthread_t worker_thread_t;
#else
typedef int worker_thread_t;
#endif

typedef struct worker_thread{
    struct pollfd* socks;
    int pollfd_capacity;
    int nsocks;
    worker_thread_t id;
#ifdef MULTITHREADED
    pthread_cond_t condvar;
    pthread_mutex_t stop_mutex;
    pthread_mutex_t nsocks_mutex;
    pthread_mutex_t poll_mutex;
#endif
} worker_thread;

typedef struct thread_pool{
    worker_thread* threads;
    int capacity;
    int size;
} thread_pool;

extern thread_pool pool;

void* worker_thread_func(void* arg);
int init_thread_pool(int capacity);
#ifdef MULTITHREADED
int start_worker_thread();
#endif
int add_curthread();
int destroy_thread_pool();

worker_thread* find_optimal_thread();
int add_fd(worker_thread* thread, int fd, short events);
int remove_fd(worker_thread* thread, int fd);
int find_fd_index(worker_thread* thread, int fd);
bool contains_fd(worker_thread* thread, int fd);
int resize_fds(worker_thread* thread);

int close_worker_threads();
int join_worker_threads();
worker_thread_t curthread_id();

#endif
