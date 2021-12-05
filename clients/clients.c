#include <stdio.h>
#include "proxy.h"
#include <stdbool.h>
#include <stdlib.h>
#include "clients.h"
#include "errcodes.h"
#include "logger/log.h"
#include "thread_pool/thread_pool.h"
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

int init_clients(){
    clients.clients = malloc(sizeof(proxy_client) * PR_CLIENTS_INIT_CAP);
    if(clients.clients){
        clients.capacity = PR_CLIENTS_INIT_CAP;
        clients.size = 0;
        return PR_SUCCESS;
    }
    else{
        clients.capacity = 0;
        clients.size = 0;
        return PR_NOT_ENOUGH_MEMORY;
    }
}

int add_client(proxy_client client){
#ifdef MULTITHREADED
    pthread_mutex_lock(&clients.mutex);
#endif
    int return_code = PR_SUCCESS;
    if(clients.capacity == clients.size)
        return_code = resize_clients();
    if(return_code == PR_NOT_ENOUGH_MEMORY){
#ifdef MULTITHREADED
        pthread_mutex_unlock(&clients.mutex);
#endif
        log_trace("THREAD %d: Could not add client with id : %d", curthread_id(), client.id);
        return PR_NOT_ENOUGH_MEMORY;
    }
    clients.clients[clients.size++] = client;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&clients.mutex);
#endif
    log_trace("THREAD %d: Successfully added client with id : %d", curthread_id(), client.id);
    return PR_SUCCESS;
}

int remove_client_by_id(int id){
#ifdef MULTITHREADED
    pthread_mutex_lock(&clients.mutex);
#endif
    bool removed = false;
    for(int i = 0; i < clients.size; i++){
        if(clients.clients[i].id == id){
            clients.clients[i] = clients.clients[--clients.size];
            removed = true;
            break;
        }
    }
#ifdef MULTITHREADED
    pthread_mutex_unlock(&clients.mutex);
#endif
    (removed) ? log_trace("THREAD %d: Successfully removed client with id : %d", curthread_id(), id) : log_trace("THREAD %d: No client with such id : %d", curthread_id(), id);
    return (removed) ? PR_SUCCESS : PR_NO_CLIENT_WITH_SUCH_ID;
}

proxy_client* find_client_by_id(int id){
#ifdef MULTITHREADED
    pthread_mutex_lock(&clients.mutex);
#endif
    for(int i = 0; i < clients.size; i++){
        if(clients.clients[i].id == id){
#ifdef MULTITHREADED
            pthread_mutex_unlock(&clients.mutex);
#endif
            log_trace("THREAD %d: Found client with such id : %d", curthread_id(), id);
            return clients.clients + i;
        }
    }
#ifdef MULTITHREADED
    pthread_mutex_unlock(&clients.mutex);
#endif
    log_trace("THREAD %d: No client with such id : %d", curthread_id(), id);
    return NULL;
}

int resize_clients(){
    proxy_client* new_ptr = realloc(clients.clients, sizeof(proxy_client) * clients.capacity * 2);
    if(new_ptr) {
        clients.clients = new_ptr;
        clients.capacity *= 2;
        log_trace("THREAD %d: Successfully resized clients array", curthread_id());
        return PR_SUCCESS;
    }
    else{
        log_trace("THREAD %d: Not enough memory for resizing clients array", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
}

int destroy_clients(){
    free(clients.clients);
}
