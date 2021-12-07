#ifndef F_GET_URL_TASK
#define F_GET_URL_TASK

#include "tasks/task_types.h"

typedef struct get_url_task{
    int (*task_func)(worker_thread*, abstract_task* task);
    int (*abort_task) (worker_thread*, struct abstract_task*);
    task_type type;
    int client_socket;
    char* get_query;
    int progress;
    bool removed;
} get_url_task;

int do_get_url_task(worker_thread* thread, abstract_task* task);
int abort_get_url_task(worker_thread* thread, abstract_task* task);
int init_get_url_task(get_url_task* task);
int free_get_url_task(get_url_task* task);

#endif
