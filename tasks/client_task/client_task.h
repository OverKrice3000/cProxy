#ifndef F_CLIENT_TASK
#define F_CLIENT_TASK

#include "cache/cache.h"
#include "tasks/task_types.h"
#include "thread_pool/thread_pool.h"
#include <sys/types.h>

#define NOT_EXECUTED 255

typedef struct client_task{
    int (*task_func) (worker_thread*, struct abstract_task*);
    int (*abort_task) (worker_thread*, struct abstract_task*);
    task_type type;
    worker_thread* last_exec;
    int client_socket;
    size_t progress;
    char* url;
    server_task* server;
    cache_entry* entry;
} client_task;

int init_client_task(client_task* task);
int free_client_task(client_task* task);
int abort_client_task(worker_thread* thread, abstract_task* task);

#include "cache_client_task/cache_client_task.h"
#include "end_client_task/end_client_task.h"

#endif
