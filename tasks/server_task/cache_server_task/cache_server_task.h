#ifndef F_CACHE_SERVER_TASK
#define F_CACHE_SERVER_TASK

int set_cache_server_task(abstract_task* task);
int do_cache_server_task(worker_thread* thread, abstract_task* task);

int server_switch_to_end_mode(worker_thread* thread, abstract_task* task);

#endif


