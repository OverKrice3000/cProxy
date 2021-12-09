#include "cache.h"
#include "proxy.h"
#include <stdio.h>
#include <sys/ddi.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "logger/log.h"
#include "errcodes.h"
#include "thread_pool/thread_pool.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

int init_cache(){
    pr_cache.entries = malloc(sizeof(cache_entry*) * PR_CACHE_INIT_CAP);
    if(!pr_cache.entries){
        pr_cache.capacity = 0;
        pr_cache.size = 0;
        return PR_NOT_ENOUGH_MEMORY;
    }
    pr_cache.capacity = PR_CACHE_INIT_CAP;
    pr_cache.size = 0;
#ifdef MULTITHREADED
    if(pthread_mutex_init(&pr_cache.add_mutex, NULL)){
        free(pr_cache.entries);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_rwlock_init(&pr_cache.remove_lock, NULL)){
        free(pr_cache.entries);
        pthread_mutex_destroy(&pr_cache.add_mutex);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_mutex_init(&pr_cache.size_mutex, NULL)){
        free(pr_cache.entries);
        pthread_mutex_destroy(&pr_cache.add_mutex);
        pthread_rwlock_destroy(&pr_cache.remove_lock);
        return PR_NOT_ENOUGH_MEMORY;
    }
#endif
    return PR_SUCCESS;
}

cache_entry* add_entry(char* key){
#ifdef MULTITHREADED
    pthread_mutex_lock(&pr_cache.add_mutex);
    pthread_rwlock_rdlock(&pr_cache.remove_lock);
#endif
    int return_code = PR_SUCCESS;
    if(pr_cache.capacity == pr_cache.size){
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_rwlock_wrlock(&pr_cache.remove_lock);
#endif
        return_code = resize_cache();
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_rwlock_rdlock(&pr_cache.remove_lock);
#endif
    }

    if(return_code == PR_NOT_ENOUGH_MEMORY){
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_mutex_unlock(&pr_cache.add_mutex);
#endif
        log_trace("THREAD %d: Could not add new cache entry. Key:\n%s", curthread_id(), key);
        return NULL;
    }
    pr_cache.entries[pr_cache.size] = malloc(sizeof(cache_entry));
    if(!pr_cache.entries[pr_cache.size]){
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_mutex_unlock(&pr_cache.add_mutex);
#endif
        log_trace("THREAD %d: Could not initialize new cache entry. Key:\n%s", curthread_id(), key);
        return NULL;
    }
    pr_cache.entries[pr_cache.size]->value = malloc(sizeof(char) * PR_ENTRY_INIT_SIZE);
    if(!pr_cache.entries[pr_cache.size]->value){
        free(pr_cache.entries[pr_cache.size]);
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_mutex_unlock(&pr_cache.add_mutex);
#endif
        log_trace("THREAD %d: Could not initialize new cache entry. Key:\n%s", curthread_id(), key);
        return NULL;
    }
#ifdef MULTITHREADED
    if(pthread_mutex_init(&pr_cache.entries[pr_cache.size]->size_mutex, NULL)){
        free(pr_cache.entries[pr_cache.size]);
        free(pr_cache.entries[pr_cache.size]->value);
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_mutex_unlock(&pr_cache.add_mutex);
        log_trace("THREAD %d: Could not initialize new cache entry. Key:\n%s", curthread_id(), key);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_rwlock_init(&pr_cache.entries[pr_cache.size]->value_lock, NULL)){
        free(pr_cache.entries[pr_cache.size]);
        free(pr_cache.entries[pr_cache.size]->value);
        pthread_mutex_destroy(&pr_cache.entries[pr_cache.size]->size_mutex);
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_mutex_unlock(&pr_cache.add_mutex);
        log_trace("THREAD %d: Could not initialize new cache entry. Key:\n%s", curthread_id(), key);
        return PR_NOT_ENOUGH_MEMORY;
    }
#endif
    pr_cache.entries[pr_cache.size]->key = key;
    pr_cache.entries[pr_cache.size]->capacity = PR_ENTRY_INIT_SIZE;
    pr_cache.entries[pr_cache.size]->size = 0;
    pr_cache.entries[pr_cache.size]->resize_cf = PR_START_RESIZE_COEF;
    pr_cache.entries[pr_cache.size]->finished = false;
    cache_entry* new_entry = pr_cache.entries[pr_cache.size];
#ifdef MULTITHREADED
    pthread_mutex_lock(&pr_cache.size_mutex);
#endif
    pr_cache.size++;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&pr_cache.size_mutex);
    pthread_rwlock_unlock(&pr_cache.remove_lock);
    pthread_mutex_unlock(&pr_cache.add_mutex);
#endif
    log_trace("THREAD %d: Successfully added new cache entry with key:\n%s", curthread_id(), key);
    return new_entry;
}

int find_entry_index_by_key(const char* const key){
#ifdef MULTITHREADED
    pthread_rwlock_rdlock(&pr_cache.remove_lock);
    pthread_mutex_lock(&pr_cache.size_mutex);
#endif
    int cur_size = pr_cache.size;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&pr_cache.size_mutex);
#endif
    for(int i = 0; i < cur_size; i++){
        if(!strcmp(key, pr_cache.entries[i]->key)){
#ifdef MULTITHREADED
            pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
            log_trace("THREAD %d: Successfully found cache entry with key:\n%s", curthread_id(), key);
            return i;
        }
    }
#ifdef MULTITHREADED
    pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
    log_trace("THREAD %d: Could not find cache entry with key:\n%s", curthread_id(), key);
    return PR_NO_SUCH_CACHE_ENTRY;
}

cache_entry* find_entry_by_key(const char* const key){
#ifdef MULTITHREADED
    pthread_rwlock_rdlock(&pr_cache.remove_lock);
    pthread_mutex_lock(&pr_cache.size_mutex);
#endif
    int cur_size = pr_cache.size;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&pr_cache.size_mutex);
#endif
    for(int i = 0; i < cur_size; i++){
        if(!strcmp(key, pr_cache.entries[i]->key)){
            cache_entry* found = pr_cache.entries[i];
#ifdef MULTITHREADED
            pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
            log_trace("THREAD %d: Successfully found cache entry with key:\n%s", curthread_id(), key);
            return found;
        }
    }
#ifdef MULTITHREADED
    pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
    log_trace("THREAD %d: Could not find cache entry with key:\n%s", curthread_id(), key);
    return NULL;
}

int append_entry(cache_entry* entry, char* data, size_t data_length){
#ifdef MULTITHREADED
    pthread_rwlock_rdlock(&pr_cache.remove_lock);
#endif
    int return_code = PR_SUCCESS;
    if(entry->capacity - entry->size < data_length){
#ifdef MULTITHREADED
        pthread_rwlock_wrlock(&entry->value_lock);
#endif
        return_code = resize_entry(entry);
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&entry->value_lock);
#endif
    }
    if(return_code == PR_NOT_ENOUGH_MEMORY){
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
        log_trace("THREAD %d: Could not append data of length %d to entry with key:\n%s", curthread_id(), data_length, entry->key);
        return PR_NOT_ENOUGH_MEMORY;
    }
    assert(entry->capacity - entry->size >= data_length);
    memcpy(entry->value + entry->size, data, data_length);
#ifdef MULTITHREADED
    pthread_mutex_lock(&entry->size_mutex);
#endif
    entry->size += data_length;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&entry->size_mutex);
    pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
}

int send_entry_to_socket(cache_entry* entry, int socket, size_t progress){
#ifdef MULTITHREADED
    pthread_rwlock_rdlock(&pr_cache.remove_lock);
    pthread_rwlock_rdlock(&entry->value_lock);
    pthread_mutex_lock(&entry->size_mutex);
#endif
    size_t cursize = entry->size;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&entry->size_mutex);
#endif
    assert(progress <= cursize);
    if(cursize == progress){
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&entry->value_lock);
        pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
        return PR_ENTRY_NO_NEW_DATA;
    }
    size_t to_send_len = min(PR_BYTES_FROM_CACHE_PER_ITERATION, cursize - progress);
    char* to_send = entry->value + progress;
    int send_val = send(socket, to_send, to_send_len, MSG_NOSIGNAL);
#ifdef MULTITHREADED
    pthread_rwlock_unlock(&entry->value_lock);
    pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
    return send_val;
}

int remove_entry_by_key(const char* const key){
    int index_to_remove = find_entry_index_by_key(key);
    assert(index_to_remove != PR_NO_SUCH_CACHE_ENTRY);
#ifdef MULTITHREADED
    pthread_rwlock_wrlock(&pr_cache.remove_lock);
    pthread_mutex_destroy(&pr_cache.entries[index_to_remove]->size_mutex);
    pthread_rwlock_destroy(&pr_cache.entries[index_to_remove]->value_lock);
#endif
    free(pr_cache.entries[index_to_remove]->value);
    free(pr_cache.entries[index_to_remove]->key);
    free(pr_cache.entries[index_to_remove]);
    pr_cache.entries[index_to_remove] = pr_cache.entries[--pr_cache.size];
#ifdef MULTITHREADED
    pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
    log_trace("THREAD %d: Successfully removed cache entry with key:\n%s", curthread_id(), key);
    return PR_SUCCESS;
}

int resize_entry(cache_entry* entry){
    size_t resize_cap = (entry->capacity < ((double)SIZE_MAX / entry->resize_cf)) ? (size_t)(entry->resize_cf * (double)entry->capacity) : SIZE_MAX;
    log_info("THREAD %d: Resizing entry with capacity %d to capacity %d, key:\n%s", curthread_id(), entry->capacity, resize_cap, entry->key);
    char* new_ptr = realloc(entry->value, sizeof(char) * resize_cap);
    if(!new_ptr){
        log_info("THREAD %d: Not enough memory for resizing entry with key:\n%s", curthread_id(), entry->key);
        return PR_NOT_ENOUGH_MEMORY;
    }
    log_info("THREAD %d: Successfully resized entry with key:\n%s", curthread_id(), entry->key);
    entry->value = new_ptr;
    entry->capacity = resize_cap;
    if(entry->resize_cf <= PR_MIN_RESIZE_COEF)
        entry->resize_cf = PR_MIN_RESIZE_COEF;
    else
        entry->resize_cf *= PR_RESIZE_COEF_DEC;
    return PR_SUCCESS;
}

int resize_cache(){
    cache_entry** new_ptr = realloc(pr_cache.entries, sizeof(cache_entry*) * pr_cache.capacity * 2);
    if(!new_ptr){
        log_trace("THREAD %d: Not enough memory for resizing cache", curthread_id());
        return PR_NOT_ENOUGH_MEMORY;
    }
    log_trace("THREAD %d: Successfully resized cache", curthread_id());
    pr_cache.entries = new_ptr;
    pr_cache.capacity *= 2;
    return PR_SUCCESS;
}

bool contains_entry(const char* const key){
    cache_entry* entry = find_entry_by_key(key);
    if(!entry)
        return false;
    return true;
}

bool contains_finished_entry(const char* const key){
    cache_entry* entry = find_entry_by_key(key);
    if(!entry)
        return false;
    if(!entry->finished)
        return false;
    return true;
}

int destroy_cache(){
    free(pr_cache.entries);
#ifdef MULTITHREADED
    pthread_mutex_destroy(&pr_cache.add_mutex);
    pthread_mutex_destroy(&pr_cache.size_mutex);
    pthread_rwlock_destroy(&pr_cache.remove_lock);
#endif
    return PR_SUCCESS;
}

bool is_entry_finished(cache_entry* entry){
    return entry->finished;
}

void set_entry_finished(cache_entry* entry){
    entry->finished = true;
}

