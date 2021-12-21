#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "proxy.h"
#include "errcodes.h"
#include "cache/cache.h"
#include "logger/log.h"
#include "socket_to_task/socket_to_task.h"
#include "thread_pool/thread_pool.h"
#include "tasks/tasks.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>

#ifdef MULTITHREADED
    #include <pthread.h>
    pthread_rwlock_t gl_abort_lock;
    pthread_mutex_t coll_server_mutex;
    pthread_mutex_t end_to_end_mutex;
    pthread_mutex_t finished_mutex;
#endif

bool end_to_end;
bool finished;

assosiations task_assosiations;
thread_pool pool;
cache pr_cache;

bool end_to_end;
bool finished;

extern char *optarg;
extern int opterr, optind, optopt;

int main(int argc, char** argv){
    end_to_end = false;
    finished = false;
    log_set_level(LOG_FATAL);
    FILE* log_file = fopen("logfile", "w+t");
    log_add_fp(log_file, LOG_TRACE);

#ifdef MULTITHREADED
    int thread_pool_capacity = PR_THREADS_DEFAULT;
    bool threads_set = false;
    char* options = "h(help)l:(log-level)e(end-to-end)t:(threads)";
#else
    int thread_pool_capacity = 1;
    char* options = "h(help)l:(log-level)e(end-to-end)";
#endif
    int symb;
    while((symb = getopt(argc, argv, options)) != -1){
        switch(symb){
            case '?' :
                perror("Option not found\n");
                break;
            case ':' :
                perror("Option misses argument\n");
                break;

            case 'l' :
                set_log_level_from_cmd();
                break;
            case 'e' :
                printf("Setting proxy to end to end mode\n");
                end_to_end = true;
                break;
#ifdef MULTITHREADED
            case 't' :
                if(atoi(optarg) > 0){
                    thread_pool_capacity = atoi(optarg);
                    threads_set = true;
                    printf("Setting thread pool capacity to %d\n", thread_pool_capacity);
                }
                break;
#endif
            case 'h' :
#ifdef MULTITHREADED
                printf("usage: ./mtserver.out [-t <threads_num> | --threads=<threads_num>] [-h | --help] [-e | --end-to-end] [-l <log_level> | --log-level=<log_level>]\n");
#else
                printf("usage: ./server.out [-h | --help] [-e | --end-to-end] [-l <log_level> | --log-level=<log_level>]\n");
#endif
                return 0;
        }
    }
#ifdef MULTITHREADED
    if(!threads_set)
        printf("Did not find threads flag! Setting thread pool capacity to default value %d\n", PR_THREADS_DEFAULT);
#endif
    if(init_logger() == PR_NOT_ENOUGH_MEMORY){
        perror("Could not allocate memory for application");
        return -1;
    }
#ifdef MULTITHREADED
    log_set_lock(logger_lock_function, NULL);
#endif
    if(init_assosiations() == PR_NOT_ENOUGH_MEMORY){
        destroy_logger();
        perror("Could not allocate memory for application");
        return -1;
    }

    if(init_thread_pool(thread_pool_capacity)){
        destroy_logger();
        destroy_assosiations();
        perror("Could not allocate memory for application");
        return -1;
    }

    if(init_cache()){
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        perror("Could not allocate memory for application");
        return -1;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(server_socket == -1){
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        destroy_cache();
        perror("Could not create server socket");
        return -1;
    }
    fcntl(server_socket, F_SETFL, fcntl(server_socket, F_GETFL) | O_NONBLOCK);

    struct sockaddr_in addr;
    addr.sin_port = htons(25566);
    addr.sin_family = AF_INET;
    memset(addr.sin_zero, 0, 8);
    int aton_val = inet_aton("10.4.0.68", &addr.sin_addr);
    assert(aton_val);

    int true_val = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &true_val, sizeof(int))){
        close(server_socket);
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        destroy_cache();
        perror("Could not set server socket option");
        return -1;
    }
    if(bind(server_socket, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))){
        close(server_socket);
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        destroy_cache();
        perror("Could not bind server socket");
        return -1;
    }
    if(listen(server_socket, 255)){
        close(server_socket);
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        destroy_cache();
        perror("Could not listen server socket");
        return -1;
    }

#ifdef MULTITHREADED
    if(pthread_rwlock_init(&gl_abort_lock, NULL)){
        close(server_socket);
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        destroy_cache();
        perror("Could not allocate memory for application");
        return -1;
    }
    if(pthread_mutex_init(&end_to_end_mutex, NULL)){
        close(server_socket);
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        destroy_cache();
        pthread_rwlock_destroy(&gl_abort_lock);
        perror("Could not allocate memory for application");
        return -1;
    }
    if(pthread_mutex_init(&finished_mutex, NULL)){
        close(server_socket);
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        destroy_cache();
        pthread_rwlock_destroy(&gl_abort_lock);
        pthread_mutex_destroy(&end_to_end_mutex);
        perror("Could not allocate memory for application");
        return -1;
    }
    if(pthread_mutex_init(&coll_server_mutex, NULL)){
        close(server_socket);
        destroy_logger();
        destroy_assosiations();
        destroy_thread_pool();
        destroy_cache();
        pthread_rwlock_destroy(&gl_abort_lock);
        pthread_mutex_destroy(&end_to_end_mutex);
        pthread_mutex_destroy(&finished_mutex);
        perror("Could not allocate memory for application");
        return -1;
    }
#endif
    struct sigaction proxintr = {
            .sa_handler = set_finished,
            .sa_flags = 0
    };
    struct sigaction pollintr = {
            .sa_handler = intrpoll,
            .sa_flags = 0
    };
    sigaction(SIGINT, &proxintr, NULL);
    sigaction(SIGUSR1, &pollintr, NULL);
#ifdef MULTITHREADED
    for(int i = 0; i < thread_pool_capacity - 1; i++){
        if(start_worker_thread() != PR_SUCCESS){
            perror("Could not start a thread");
            pthread_rwlock_destroy(&gl_abort_lock);
            pthread_rwlock_destroy(&gl_abort_lock);
            pthread_mutex_destroy(&end_to_end_mutex);
            close_worker_threads();
            close(server_socket);
            destroy_logger();
            destroy_assosiations();
            destroy_thread_pool();
            destroy_cache();
            return -1;
        }
    }
#endif
    add_curthread();

    listen_task task;
    init_listen_task(&task);
    task.server_socket = server_socket;
    add_fd(pool.threads + pool.size - 1, server_socket, POLLIN);

    add_assosiation(server_socket, (abstract_task*)&task);

    worker_thread_func(pool.threads + pool.size - 1);
    join_worker_threads();

    log_info("THREAD %d: Joined other threads", curthread_id());

    bool tasks_left = true;
    while(tasks_left){
        for(int i = 0; i < pool.size; i++){
            for(int j = pool.threads[i].nsocks - 1; j >= 0; j--){
                abstract_task* task = find_assosiation_by_sock(pool.threads[i].socks[j].fd)->task;
                task->abort_task(pool.threads + i, task);
            }
        }
        tasks_left = false;
        for(int i = 0; i < pool.size; i++) {
            if(pool.threads[i].nsocks != 0){
                tasks_left = true;
                break;
            }
        }
    }

    destroy_cache();
    destroy_thread_pool();
    destroy_logger();
    destroy_assosiations();
#ifdef MULTITHREADED
    pthread_rwlock_destroy(&gl_abort_lock);
    pthread_rwlock_destroy(&gl_abort_lock);
    pthread_mutex_destroy(&end_to_end_mutex);
    pthread_mutex_destroy(&finished_mutex);
    pthread_mutex_destroy(&coll_server_mutex);
#endif
	return 0;
}

void intrpoll(int signal){}

bool is_end_to_end(){
#ifdef MULTITHREADED
    pthread_mutex_lock(&end_to_end_mutex);
#endif
    bool cur_end_to_end = end_to_end;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&end_to_end_mutex);
#endif
    return cur_end_to_end;
}

bool is_finished(){
#ifdef MULTITHREADED
    pthread_mutex_lock(&finished_mutex);
#endif
    bool cur_finished = finished;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&finished_mutex);
#endif
    return cur_finished;
}

void set_end_to_end(){
#ifdef MULTITHREADED
    pthread_mutex_lock(&end_to_end_mutex);
#endif
    end_to_end = true;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&end_to_end_mutex);
#endif
}

void set_finished(int signal){
#ifdef MULTITHREADED
    pthread_mutex_lock(&finished_mutex);
#endif
    finished = true;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&finished_mutex);
#endif
}

void set_log_level_from_cmd(){
    if(!strcmp("trace", optarg)){
        printf("Setting log level to trace\n");
        log_set_level(LOG_TRACE);
    }
    else if(!strcmp("debug", optarg)){
        printf("Setting log level to debug\n");
        log_set_level(LOG_DEBUG);
    }
    else if(!strcmp("info", optarg)){
        printf("Setting log level to info\n");
        log_set_level(LOG_INFO);
    }
    else if(!strcmp("warn", optarg)){
        printf("Setting log level to warn\n");
        log_set_level(LOG_WARN);
    }
    else if(!strcmp("error", optarg)){
        printf("Setting log level to error\n");
        log_set_level(LOG_ERROR);
    }
    else if(!strcmp("fatal", optarg)){
        printf("Setting log level to fatal\n");
        log_set_level(LOG_FATAL);
    }
    else
        perror("log level option: bad argument");
}
