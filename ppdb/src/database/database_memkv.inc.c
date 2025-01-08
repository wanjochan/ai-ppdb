#include "internal/database.h"
#include "internal/base.h"

typedef struct ppdb_database_memkv_entry {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    uint64_t version;
    struct ppdb_database_memkv_entry* next;
} ppdb_database_memkv_entry_t;

typedef struct ppdb_database_memkv {
    ppdb_database_memkv_entry_t* entries;
    size_t size;
    pthread_mutex_t mutex;
    pthread_rwlock_t rwlock;
} ppdb_database_memkv_t;

static int database_memkv_init(ppdb_database_memkv_t** memkv) {
    if (!memkv) {
        return PPDB_ERR_PARAM;
    }

    *memkv = (ppdb_database_memkv_t*)calloc(1, sizeof(ppdb_database_memkv_t));
    if (!*memkv) {
        return PPDB_ERR_NOMEM;
    }

    (*memkv)->entries = NULL;
    (*memkv)->size = 0;

    pthread_mutex_init(&(*memkv)->mutex, NULL);
    pthread_rwlock_init(&(*memkv)->rwlock, NULL);

    return PPDB_OK;
}

static void database_memkv_cleanup(ppdb_database_memkv_t* memkv) {
    if (!memkv) {
        return;
    }

    ppdb_database_memkv_entry_t* current = memkv->entries;
    while (current) {
        ppdb_database_memkv_entry_t* next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }

    pthread_mutex_destroy(&memkv->mutex);
    pthread_rwlock_destroy(&memkv->rwlock);
}

int ppdb_database_memkv_put(ppdb_database_memkv_t* memkv, const void* key, size_t key_size, const void* value, size_t value_size, uint64_t version) {
    if (!memkv || !key || !value) {
        return PPDB_ERR_PARAM;
    }

    ppdb_database_memkv_entry_t* entry = (ppdb_database_memkv_entry_t*)malloc(sizeof(ppdb_database_memkv_entry_t));
    if (!entry) {
        return PPDB_ERR_NOMEM;
    }

    entry->key = malloc(key_size);
    entry->value = malloc(value_size);
    if (!entry->key || !entry->value) {
        free(entry->key);
        free(entry->value);
        free(entry);
        return PPDB_ERR_NOMEM;
    }

    memcpy(entry->key, key, key_size);
    memcpy(entry->value, value, value_size);
    entry->key_size = key_size;
    entry->value_size = value_size;
    entry->version = version;

    pthread_rwlock_wrlock(&memkv->rwlock);
    entry->next = memkv->entries;
    memkv->entries = entry;
    memkv->size++;
    pthread_rwlock_unlock(&memkv->rwlock);

    return PPDB_OK;
}

int ppdb_database_memkv_get(ppdb_database_memkv_t* memkv, const void* key, size_t key_size, void** value, size_t* value_size, uint64_t* version) {
    if (!memkv || !key || !value || !value_size || !version) {
        return PPDB_ERR_PARAM;
    }

    pthread_rwlock_rdlock(&memkv->rwlock);

    ppdb_database_memkv_entry_t* current = memkv->entries;
    while (current) {
        if (current->key_size == key_size && memcmp(current->key, key, key_size) == 0) {
            *value = malloc(current->value_size);
            if (!*value) {
                pthread_rwlock_unlock(&memkv->rwlock);
                return PPDB_ERR_NOMEM;
            }
            memcpy(*value, current->value, current->value_size);
            *value_size = current->value_size;
            *version = current->version;
            pthread_rwlock_unlock(&memkv->rwlock);
            return PPDB_OK;
        }
        current = current->next;
    }

    pthread_rwlock_unlock(&memkv->rwlock);
    return PPDB_ERR_NOT_FOUND;
}

void ppdb_database_memkv_destroy(ppdb_database_memkv_t* memkv) {
    if (!memkv) {
        return;
    }

    database_memkv_cleanup(memkv);
    free(memkv);
} 