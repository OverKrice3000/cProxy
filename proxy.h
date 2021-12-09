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
#define PR_BYTES_FROM_CACHE_PER_ITERATION 8192

extern bool end_to_end;
extern bool finished;

bool is_end_to_end();
bool is_finished();

void set_end_to_end();
void set_finished();

#endif
