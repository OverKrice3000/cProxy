#include "cache.h"
#include "proxy.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "logger/log.h"
#include "errcodes.h"
#include "thread_pool/thread_pool.h"
#include <assert.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

int init_cache(){
    pr_cache.entries = malloc(sizeof(cache_entry) * PR_CACHE_INIT_CAP);
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

int add_entry(char* key){
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
        return PR_NOT_ENOUGH_MEMORY;
    }

    pr_cache.entries[pr_cache.size].value = malloc(sizeof(char) * PR_ENTRY_INIT_SIZE);
    if(!pr_cache.entries[pr_cache.size].value){
#ifdef MULTITHREADED
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_mutex_unlock(&pr_cache.add_mutex);
#endif
        log_trace("THREAD %d: Could not initialize new cache entry. Key:\n%s", curthread_id(), key);
        return PR_NOT_ENOUGH_MEMORY;
    }
#ifdef MULTITHREADED
    if(pthread_mutex_init(&pr_cache.entries[pr_cache.size].size_mutex, NULL)){
        free(pr_cache.entries[pr_cache.size].value);
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_mutex_unlock(&pr_cache.add_mutex);
        log_trace("THREAD %d: Could not initialize new cache entry. Key:\n%s", curthread_id(), key);
        return PR_NOT_ENOUGH_MEMORY;
    }
    if(pthread_rwlock_init(&pr_cache.entries[pr_cache.size].value_lock, NULL)){
        free(pr_cache.entries[pr_cache.size].value);
        pthread_mutex_destroy(&pr_cache.entries[pr_cache.size].size_mutex);
        pthread_rwlock_unlock(&pr_cache.remove_lock);
        pthread_mutex_unlock(&pr_cache.add_mutex);
        log_trace("THREAD %d: Could not initialize new cache entry. Key:\n%s", curthread_id(), key);
        return PR_NOT_ENOUGH_MEMORY;
    }
#endif
    pr_cache.entries[pr_cache.size].key = key;
    pr_cache.entries[pr_cache.size].capacity = PR_ENTRY_INIT_SIZE;
    pr_cache.entries[pr_cache.size].size = 0;
    pr_cache.entries[pr_cache.size].resize_cf = PR_START_RESIZE_COEF;
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
    return PR_SUCCESS;
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
        if(!strcmp(key, pr_cache.entries[i].key)){
#ifdef MULTITHREADED
            pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
            log_trace("THREAD %d: Successfully found cache entry with key:\n%s", curthread_id(), key);
            return pr_cache.entries + i;
        }
    }
#ifdef MULTITHREADED
    pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
    log_trace("THREAD %d: Could not find cache entry with key:\n%s", curthread_id(), key);
    return NULL;
}

int append_entry(cache_entry* entry, char* data, int data_length){
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
    strncpy(entry->value + entry->size, data, data_length);
#ifdef MULTITHREADED
    pthread_mutex_lock(&entry->size_mutex);
#endif
    entry->size += data_length;
#ifdef MULTITHREADED
    pthread_mutex_unlock(&entry->size_mutex);
    pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
}

int remove_entry_by_key(const char* const key){
    cache_entry* to_remove = find_entry_by_key(key);
    assert(to_remove);
    if(!to_remove){
        log_fatal("THREAD %d: Could not find cache entry with key:\n%s", curthread_id(), key);
        return PR_NO_SUCH_CACHE_ENTRY;
    }
#ifdef MULTITHREADED
    pthread_rwlock_wrlock(&pr_cache.remove_lock);
    pthread_mutex_destroy(&to_remove->size_mutex);
    pthread_rwlock_destroy(&to_remove->value_lock);
#endif
    free(to_remove->value);
    free(to_remove->key);
    int entr_index = to_remove - pr_cache.entries;
    pr_cache.entries[entr_index] = pr_cache.entries[--pr_cache.size];
#ifdef MULTITHREADED
    pthread_rwlock_unlock(&pr_cache.remove_lock);
#endif
    return PR_SUCCESS;
}

int resize_entry(cache_entry* entry){
    char* new_ptr = realloc(entry->value, sizeof(char) * entry->resize_cf * entry->capacity);
    if(!new_ptr){
        log_trace("THREAD %d: Not enough memory for resizing entry with key:\n%s", curthread_id(), entry->key);
        return PR_NOT_ENOUGH_MEMORY;
    }
    log_trace("THREAD %d: Successfully resized entry with key:\n%s", curthread_id(), entry->key);
    entry->value = new_ptr;
    entry->capacity *= entry->resize_cf;
    entry->resize_cf *= PR_RESIZE_COEF_DEC;
    return PR_SUCCESS;
}

int resize_cache(){
    cache_entry* new_ptr = realloc(pr_cache.entries, sizeof(cache_entry) * pr_cache.capacity * 2);
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
    if(entry)
        return true;
    else
        return false;
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

