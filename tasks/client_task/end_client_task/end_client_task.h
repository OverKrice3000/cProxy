#ifndef F_END_CLIENT_TASK
#define F_END_CLIENT_TASK

int set_end_client_task(abstract_task* task);
int do_end_client_task(worker_thread* thread, abstract_task* task);
int add_server_task_fd(worker_thread* thread, abstract_task* task);

#endif
