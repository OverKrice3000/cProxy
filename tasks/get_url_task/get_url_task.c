#include <stdio.h>
#include "logger/log.h"
#include "socket_to_task/socket_to_task.h"
#include "clients/clients.h"
#include "thread_pool/thread_pool.h"
#include "errcodes.h"
#include <assert.h>

int do_get_url_task(worker_thread* thread, abstract_task* task){
    return abort_get_url_task(thread, task);
}

int abort_get_url_task(worker_thread* thread, abstract_task* task){
    get_url_task* dec_task = (get_url_task*)task;
    int ass_val = remove_assosiation_by_sock(dec_task->client_socket);
    assert(ass_val == PR_SUCCESS);
    fprintf(stderr, "SOCKET %d CLIENT %d\n", dec_task->client_socket, dec_task->client_id);
    int client_val = remove_client_by_id(dec_task->client_id);
    assert(client_val == PR_SUCCESS);
    free(dec_task->get_query);
    int fd_val = remove_fd(thread, dec_task->client_socket);
    assert(fd_val == PR_SUCCESS);
    close(dec_task->client_socket);
    return PR_SUCCESS;
}

int init_get_url_task(get_url_task* task){
    task->task_func = do_get_url_task;
    task->abort_task = abort_get_url_task;
    task->type = GET_URL_TASK;
    task->progress = 0;
    task->get_query = malloc(sizeof(char) * GET_MAX_LENGTH);
    if(!task->get_query){
        log_trace("THREAD %d: not enough memory to initialize get_url_task!", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    return PR_SUCCESS;
}

int free_get_url_task(get_url_task* task){
    free(task->get_query);
}
