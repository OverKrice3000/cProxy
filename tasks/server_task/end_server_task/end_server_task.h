#ifndef F_END_SERVER_TASK
#define F_END_SERVER_TASK

int set_end_server_task(abstract_task* task);
int do_end_server_task(worker_thread* thread, abstract_task* task);

#endif
