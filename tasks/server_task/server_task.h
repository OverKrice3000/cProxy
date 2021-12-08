#ifndef F_SERVER_TASK
#define F_SERVER_TASK

#include "cache/cache.h"
#include "tasks/task_types.h"
#include "thread_pool/thread_pool.h"
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

struct client_task;

typedef struct server_task{
    int (*task_func) (worker_thread*, struct abstract_task*);
    int (*abort_task) (worker_thread*, struct abstract_task*);
    task_type type;
    int server_socket;
    int progress;
    bool aborted;
    char* end_buf;
    int end_progress;
    int end_capacity;
    int end_clients_reading;
    char* query;
    char* url;
    int query_length;
    struct client_task** clients;
    int clients_size;
    int clients_capacity;
    cache_entry* entry;
#ifdef MULTITHREADED
    pthread_mutex_t clients_mutex;
#endif
} server_task;

int init_server_task(server_task* task, char* query);
int free_server_task(server_task* task);
int abort_server_task(worker_thread* thread, abstract_task* task);
int add_server_task_client(server_task* server, struct client_task* client);
int resize_server_task_clients(server_task* server);
int add_client_tasks_fd(worker_thread* thread, abstract_task* task);

bool is_server_aborted(server_task* task);
void set_server_aborted(server_task* task);

#include "cache_server_task/cache_server_task.h"
#include "end_server_task/end_server_task.h"
#include "connect_task/connect_task.h"

#endif
