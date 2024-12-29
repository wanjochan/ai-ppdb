#include "sync.h"

void ppdb_stripe_locks_init(ppdb_stripe_locks_t* locks, size_t count) {
    locks->count = count;
    locks->locks = (pthread_mutex_t*)malloc(count * sizeof(pthread_mutex_t));
    for (size_t i = 0; i < count; i++) {
        pthread_mutex_init(&locks->locks[i], NULL);
    }
}

void ppdb_stripe_locks_destroy(ppdb_stripe_locks_t* locks) {
    if (!locks) return;
    if (locks->locks) {
        for (size_t i = 0; i < locks->count; i++) {
            pthread_mutex_destroy(&locks->locks[i]);
        }
        free(locks->locks);
        locks->locks = NULL;
    }
    locks->count = 0;
}

static size_t hash_key(const void* key, size_t key_len) {
    const unsigned char* data = (const unsigned char*)key;
    size_t hash = 5381;
    for (size_t i = 0; i < key_len; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

void ppdb_stripe_locks_lock(ppdb_stripe_locks_t* locks, const void* key, size_t key_len) {
    if (!locks || !locks->locks || !locks->count) return;
    size_t index = hash_key(key, key_len) % locks->count;
    pthread_mutex_lock(&locks->locks[index]);
}

void ppdb_stripe_locks_unlock(ppdb_stripe_locks_t* locks, const void* key, size_t key_len) {
    if (!locks || !locks->locks || !locks->count) return;
    size_t index = hash_key(key, key_len) % locks->count;
    pthread_mutex_unlock(&locks->locks[index]);
}
