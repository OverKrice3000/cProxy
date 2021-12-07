#ifndef F_SERVER_TASK
#define F_SERVER_TASK

struct client_task;

typedef struct server_task{
    int (*task_func) (worker_thread*, struct abstract_task*);
    int (*abort_task) (worker_thread*, struct abstract_task*);
    task_type type;
    int server_socket;
    struct client_task** clients;
    int progress;
    bool aborted;
    bool finished;
    char* end_buf;
    char* query;
} server_task;

int init_server_task(server_task* task, char* query);
int free_server_task(server_task* task);


#include "cache_server_task/cache_server_task.h"
#include "end_server_task/end_server_task.h"
#include "connect_task/connect_task.h"

#endif
