#ifndef F_SOCK_TO_TASK
#define F_SOCK_TO_TASK

#include <stdio.h>
#include "tasks/tasks.h"
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

typedef struct sock_task_assosiation{
    int socket;
    abstract_task* task;
} assosiation;

typedef struct sock_task_assosiations{
    assosiation** assosiations;
    int capacity;
    int size;
#ifdef MULTITHREADED
    pthread_mutex_t mutex;
#endif
} assosiations;

extern assosiations task_assosiations;

int init_assosiations();
int add_assosiation(int socket, abstract_task* task);
int remove_assosiation_by_sock(int sock);
assosiation* find_assosiation_by_sock(int sock);
int resize_assosiations();
int destroy_assosiations();

#ifdef MULTITHREADED
void lock_assosiations();
void unlock_assosiations();
#endif

#endif
