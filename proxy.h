#ifndef F_PROXY
#define F_PROXY

#include <stdbool.h>

#define GET_MAX_LENGTH 8192
#define PR_CACHE_INIT_CAP 16
#define PR_ENTRY_INIT_SIZE 8192
#define PR_START_RESIZE_COEF 3.0
#define PR_RESIZE_COEF_DEC 0.9
#define PR_MIN_RESIZE_COEF 1.2
#define PR_CLIENTS_INIT_CAP 16
#define PR_ASSOSIATIONS_INIT_CAP 16
#define PR_POLLFD_INIT_CAPACITY 16
#define PR_SERVER_CLIENTS_INIT_CAP 16
#define PR_GETHOST_BUFSIZ 8192
#define PR_END_SERVER_BUFSIZ 8192
#define PR_BYTES_TO_CACHE_PER_ITERATION 65536
#define PR_BYTES_FROM_CACHE_PER_ITERATION 65536
#define PR_THREADS_DEFAULT 4

bool is_end_to_end();
bool is_finished();

void set_end_to_end();
void set_finished(int);

void intrpoll(int signal);

void set_log_level_from_cmd();

#ifdef MULTITHREADED
    #include <pthread.h>
    extern pthread_rwlock_t gl_abort_lock;
    extern pthread_mutex_t coll_server_mutex;
#endif

#endif
