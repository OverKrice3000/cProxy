#ifndef F_CACHE
#define F_CACHE

#include <stdbool.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

typedef struct cache_entry{
    char* key;
    char* value;
    int capacity;
    int size;
    double resize_cf;
    bool finished;
#ifdef MULTITHREADED
    pthread_mutex_t size_mutex;
    pthread_rwlock_t value_lock;
#endif
} cache_entry;

typedef struct cache{
    cache_entry** entries;
    int capacity;
    int size;
#ifdef MULTITHREADED
    pthread_mutex_t add_mutex;
    pthread_mutex_t size_mutex;
    pthread_rwlock_t remove_lock;
#endif
} cache;

extern cache pr_cache;

int init_cache();
cache_entry* add_entry(char* key);
cache_entry* find_entry_by_key(const char* const key);
int append_entry(cache_entry* entry, char* data, int data_length);
int send_entry_to_socket(cache_entry* entry, int socket, int progress);
int remove_entry_by_key(const char* const key);
int resize_entry(cache_entry* entry);
int resize_cache();
bool contains_entry(const char* const key);
bool contains_finished_entry(const char* const key);
int destroy_cache();
bool is_entry_finished(cache_entry* entry);
void set_entry_finished(cache_entry* entry);

#endif
