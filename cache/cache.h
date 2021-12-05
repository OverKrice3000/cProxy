#include <stdbool.h>

#ifdef MULTITHREADED
    #include <pthread.h>
#endif


typedef struct cache_entry{
    char* key;
    char* value;
    int capacity;
    int size;
    int resize_cf;
#ifdef MULTITHREADED
    pthread_mutex_t size_mutex;
    pthread_rwlock_t value_lock;
#endif
} cache_entry;

typedef struct cache{
    cache_entry* entries;
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
int add_entry(char* key);
cache_entry* find_entry_by_key(const char* const key);
int append_entry(cache_entry* entry, char* data, int data_length);
int remove_entry_by_key(const char* const key);
int resize_entry(cache_entry* entry);
int resize_cache();
bool contains_entry(const char* const key);
int destroy_cache();
