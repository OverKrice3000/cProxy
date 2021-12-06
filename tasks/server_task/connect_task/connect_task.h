#ifndef F_CONNECT_TASK
#define F_CONNECT_TASK

#include "tasks/task_types.h"

int set_connect_task(abstract_task* task);
int do_connect_task(worker_thread* thread, abstract_task* task);
int abort_connect_task(worker_thread* thread, abstract_task* task);
int finalize_connect_task(worker_thread* thread, abstract_task* task);

#endif
