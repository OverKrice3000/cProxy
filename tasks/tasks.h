#ifndef F_TASKS
#define F_TASKS

#include "tasks/task_types.h"
#include "thread_pool/thread_pool.h"

typedef struct abstract_task{
    int (*task_func) (worker_thread*, struct abstract_task*);
    int (*abort_task) (worker_thread*, struct abstract_task*);
    task_type type;
} abstract_task;

#include "listen_task/listen_task.h"
#include "get_url_task/get_url_task.h"
#include "server_task/server_task.h"
#include "client_task/client_task.h"

#endif
