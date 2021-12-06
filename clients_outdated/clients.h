#ifndef F_CLIENTS
#define F_CLIENTS

#include <stdio.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif


typedef struct proxy_client{
    int id;
    int sock;
    char* url;
} proxy_client;

typedef struct proxy_clients{
    proxy_client* clients;
    int capacity;
    int size;
#ifdef MULTITHREADED
    pthread_mutex_t mutex;
#endif
} proxy_clients;

extern proxy_clients clients;

int init_clients();
int add_client(proxy_client client);
int remove_client_by_id(int id);
proxy_client* find_client_by_id(int id);
int resize_clients();
int destroy_clients();

#endif
