#ifndef F_LISTEN_TASK
#define F_LISTEN_TASK

#include "tasks/task_types.h"
#include "thread_pool/thread_pool.h"

typedef struct listen_task{
    int (*task_func)(worker_thread*, abstract_task* task);
    int (*abort_task) (worker_thread*, struct abstract_task*);
    task_type type;
    int server_socket;
} listen_task;

int do_listen_task(worker_thread* thread, abstract_task* task);
int abort_listen_task(worker_thread* thread, abstract_task* task);
int init_listen_task(listen_task* task);

#endif F_LISTEN_TASK
