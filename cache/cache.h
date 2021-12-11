#ifndef F_CACHE
#define F_CACHE

#include <stdbool.h>
#include <sys/types.h>
#ifdef MULTITHREADED
    #include <pthread.h>
#endif

typedef struct cache_entry{
    char* key;
    char* value;
    size_t capacity;
    size_t size;
    double resize_cf;
    bool finished;
#ifdef MULTITHREADED
    pthread_mutex_t size_mutex;
    pthread_rwlock_t value_lock;
#endif
} cache_entry;

typedef struct cache{
    cache_entry** entries;
    size_t capacity;
    size_t size;
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
int append_entry(cache_entry* entry, char* data, size_t data_length);
int recv_entry_from_socket(cache_entry* entry, int socket);
int send_entry_to_socket(cache_entry* entry, int socket, size_t progress);
int remove_entry_by_key(const char* const key);
int resize_entry(cache_entry* entry);
int resize_cache();
bool contains_entry(const char* const key);
bool contains_finished_entry(const char* const key);
int destroy_cache();
bool is_entry_finished(cache_entry* entry);
void set_entry_finished(cache_entry* entry);

#endif
