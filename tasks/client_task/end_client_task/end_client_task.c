#include "tasks/tasks.h"
#include "thread_pool/thread_pool.h"
#include "logger/log.h"

int set_end_client_task(abstract_task* task){
    task->task_func = do_end_client_task;
    task->abort_task = abort_end_client_task;
    task->type = END_CLIENT_TASK;
    return PR_SUCCESS;
}

int do_end_client_task(worker_thread* thread, abstract_task* task){
    log_trace("THREAD %d: IM DOING CLIENT TASK", thread->id);
    client_task* dec_task = (client_task*)task;
    assert(remove_fd(thread, dec_task->client_socket) == PR_SUCCESS);
    assert(remove_assosiation_by_sock(dec_task->client_socket) == PR_SUCCESS);
    free(task);
    return 0;
}

int abort_end_client_task(worker_thread* thread, abstract_task* task){
    return 0;
}

int finalize_end_client_task(worker_thread* thread, abstract_task* task){
    return 0;
}
