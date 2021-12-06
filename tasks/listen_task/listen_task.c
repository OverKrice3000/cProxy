#include "tasks/tasks.h"
#include "errcodes.h"
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include "thread_pool/thread_pool.h"
#include "socket_to_task/socket_to_task.h"
#include "logger/log.h"
#include <unistd.h>
#include <errno.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

int do_listen_task(worker_thread* thread, abstract_task* task){
    listen_task* dec_task = (listen_task*)task;
    int new_sock = accept(dec_task->server_socket, NULL, NULL);
    if(new_sock == -1){
        if(errno == ENOMEM){
            log_trace("THREAD %d: Not enough memory to accept connection!", curthread_id());
            return PR_NOT_ENOUGH_MEMORY;
        }
        log_trace("THREAD %d: Could not accept new connection", curthread_id());
        return PR_CONTINUE;
    }

    get_url_task* new_url_task = malloc(sizeof(get_url_task));
    if(!new_url_task){
        close(new_sock);
        log_trace("THREAD %d: Not enough memory to create task!", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    int init_val = init_get_url_task(new_url_task);
    if(init_val == PR_NOT_ENOUGH_MEMORY){
        close(new_sock);
        free(new_url_task);
        log_trace("THREAD %d: Not enough memory to create task!", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    new_url_task->client_socket = new_sock;

    assosiation new_assosiation = {
            .socket = new_sock,
            .task = (abstract_task*) new_url_task
    };
    int ass_val = add_assosiation(new_assosiation);
    if(ass_val == PR_NOT_ENOUGH_MEMORY){
        close(new_sock);
        free_get_url_task(new_url_task);
        free(new_url_task);
        log_trace("THREAD %d: Not enough memory to add new assosiation!", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }

    worker_thread* opt = find_optimal_thread();
    int fd_val = add_fd(opt, new_sock, POLLIN);
    if(fd_val == PR_NOT_ENOUGH_MEMORY){
        close(new_sock);
        free_get_url_task(new_url_task);
        free(new_url_task);
        remove_assosiation_by_sock(new_sock);
        log_trace("THREAD %d: Not enough memory to add fd to thread %d!", curthread_id(), opt->id);
        return PR_NOT_ENOUGH_MEMORY;
    }
#ifdef MULTITHREADED
    pthread_cond_signal(&opt->condvar);
#endif

    log_info("THREAD %d: Accepted new connection. Socket : %d.", curthread_id(), new_sock);
    return PR_SUCCESS;
}

int init_listen_task(listen_task* task){
    task->task_func = do_listen_task;
    task->abort_task = abort_listen_task;
    task->type = LISTEN_TASK;
    return PR_SUCCESS;
}

int abort_listen_task(worker_thread* thread, abstract_task* task){
    listen_task* dec_task = (listen_task*)task;
    close(dec_task->server_socket);
    remove_assosiation_by_sock(dec_task->server_socket);
    remove_fd(thread, dec_task->server_socket);
    return PR_SUCCESS;
}
