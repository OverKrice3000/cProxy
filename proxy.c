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

assosiations task_assosiations;
thread_pool pool;
cache pr_cache;

bool end_to_end;
bool finished;

#ifdef MULTITHREADED
    pthread_mutex_t temp_mutex;
#endif

int main(int argc, char** argv){
#ifdef MULTITHREADED
    pthread_rwlock_init(&gl_abort_lock, NULL);
#endif
    end_to_end = true;
    finished = false;
    sigset(SIGINT, set_finished);

    int thread_pool_capacity = 1;
#ifdef MULTITHREADED
    if(argc != 2){
        perror("bad arguments");
        return -1;
    }
    thread_pool_capacity = atoi(argv[1]);
    if(thread_pool_capacity <= 0){
        perror("bad arguments");
        return -1;
    }
#endif
    if(init_logger() == PR_NOT_ENOUGH_MEMORY){
        perror("Could not allocate memory for application");
        return -1;
    }
    log_set_level(LOG_TRACE);
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
    for(int i = 0; i < thread_pool_capacity - 1; i++){
        if(start_worker_thread() != PR_SUCCESS){
            perror("Could not start a thread");
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
	return 0;
}

bool is_end_to_end(){
    return end_to_end;
}

bool is_finished(){
    return finished;
}

void set_end_to_end(){
    end_to_end = true;
}

void set_finished(int signal){
    finished = true;
}
