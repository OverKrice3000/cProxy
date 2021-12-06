#include "tasks/tasks.h"

int init_client_task(client_task* task){
    task->progress = 0;
    task->server = NULL;
    return PR_SUCCESS;
}

int free_client_task(client_task* task){
    return PR_SUCCESS;
}

#include "cache_client_task/cache_client_task.c"
#include "end_client_task/end_client_task.c"
