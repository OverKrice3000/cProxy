#ifndef F_CACHE_CLIENT_TASK
#define F_CACHE_CLIENT_TASK

int set_cache_client_task(abstract_task* task);
int do_cache_client_task(worker_thread* thread, abstract_task* task);

int client_switch_to_end_mode(worker_thread* thread, abstract_task* task);

#endif


