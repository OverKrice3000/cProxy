#ifndef F_CLIENT_TASK
#define F_CLIENT_TASK

typedef struct client_task{
    int (*task_func) (worker_thread*, struct abstract_task*);
    int (*abort_task) (worker_thread*, struct abstract_task*);
    task_type type;
    int client_socket;
    int progress;
    char* url;
    server_task* server;
} client_task;

int init_client_task(client_task* task);
int free_client_task(client_task* task);

#include "cache_client_task/cache_client_task.h"
#include "end_client_task/end_client_task.h"

#endif
