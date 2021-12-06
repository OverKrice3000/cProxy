#include <stdio.h>
#include "tasks/tasks.h"
#include "logger/log.h"

int set_connect_task(abstract_task* task){
    task->task_func = do_connect_task;
    task->abort_task = abort_connect_task;
    task->type = CONNECT_TASK;
    return PR_SUCCESS;
}

int do_connect_task(worker_thread* thread, abstract_task* task){
    log_trace("THREAD %d: IM DOING CONNECT TASK", thread->id);
    server_task* dec_task = (server_task*)task;
    assert(remove_fd(thread, dec_task->server_socket) == PR_SUCCESS);
    free(dec_task->query);
    free_server_task(dec_task);
    assert(remove_assosiation_by_sock(dec_task->server_socket) == PR_SUCCESS);
    free(task);
    return 0;
}

int abort_connect_task(worker_thread* thread, abstract_task* task){
    return 0;
}

int finalize_connect_task(worker_thread* thread, abstract_task* task){
    return 0;
}
