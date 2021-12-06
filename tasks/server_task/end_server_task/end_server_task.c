#include "tasks/tasks.h"
#include "thread_pool/thread_pool.h"
#include "logger/log.h"

int set_end_server_task(abstract_task* task){
    task->task_func = do_end_server_task;
    task->abort_task = abort_end_server_task;
    task->type = END_SERVER_TASK;
}

int do_end_server_task(worker_thread* thread, abstract_task* task){
    return 0;
}

int abort_end_server_task(worker_thread* thread, abstract_task* task){
    return 0;
}

int finalize_end_server_task(){
    return 0;
}
