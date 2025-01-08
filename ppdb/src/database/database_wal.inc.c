#include "internal/database.h"
#include "internal/base.h"

typedef struct ppdb_database_wal_entry {
    uint64_t txn_id;
    uint32_t type;
    uint32_t table_id;
    uint32_t key_size;
    uint32_t value_size;
    char data[];  // key followed by value
} ppdb_database_wal_entry_t;

typedef struct ppdb_database_wal {
    char* path;
    int fd;
    uint64_t size;
    ppdb_base_mutex_t mutex;
} ppdb_database_wal_t;

static int database_wal_init(ppdb_database_wal_t** wal, const char* path) {
    if (!wal || !path) {
        return PPDB_ERR_PARAM;
    }

    *wal = (ppdb_database_wal_t*)calloc(1, sizeof(ppdb_database_wal_t));
    if (!*wal) {
        return PPDB_ERR_NOMEM;
    }

    (*wal)->path = strdup(path);
    if (!(*wal)->path) {
        free(*wal);
        return PPDB_ERR_NOMEM;
    }

    (*wal)->fd = open(path, O_CREAT | O_RDWR | O_APPEND, 0644);
    if ((*wal)->fd < 0) {
        free((*wal)->path);
        free(*wal);
        return PPDB_ERR_IO;
    }

    ppdb_base_mutex_create(&(*wal)->mutex, NULL);
    (*wal)->size = lseek((*wal)->fd, 0, SEEK_END);

    return PPDB_OK;
}

static void database_wal_cleanup(ppdb_database_wal_t* wal) {
    if (!wal) {
        return;
    }

    if (wal->fd >= 0) {
        close(wal->fd);
    }

    if (wal->path) {
        free(wal->path);
    }

    ppdb_base_mutex_destroy(&wal->mutex);
}

int ppdb_database_wal_append(ppdb_database_wal_t* wal, ppdb_database_txn_t* txn,
                           uint32_t type, uint32_t table_id,
                           const void* key, size_t key_size,
                           const void* value, size_t value_size) {
    if (!wal || !txn || !key || (value_size > 0 && !value)) {
        return PPDB_ERR_PARAM;
    }

    size_t entry_size = sizeof(ppdb_database_wal_entry_t) + key_size + value_size;
    ppdb_database_wal_entry_t* entry = malloc(entry_size);
    if (!entry) {
        return PPDB_ERR_NOMEM;
    }

    // Fill entry header
    entry->txn_id = txn->snapshot->txn_id;
    entry->type = type;
    entry->table_id = table_id;
    entry->key_size = key_size;
    entry->value_size = value_size;

    // Copy key and value
    memcpy(entry->data, key, key_size);
    if (value_size > 0) {
        memcpy(entry->data + key_size, value, value_size);
    }

    // Write to WAL file
    ppdb_base_mutex_lock(&wal->mutex);
    ssize_t written = write(wal->fd, entry, entry_size);
    if (written < 0 || (size_t)written != entry_size) {
        ppdb_base_mutex_unlock(&wal->mutex);
        free(entry);
        return PPDB_ERR_IO;
    }

    // Force sync to disk
    fsync(wal->fd);

    wal->size += entry_size;
    ppdb_base_mutex_unlock(&wal->mutex);

    free(entry);
    return PPDB_OK;
}

int ppdb_database_wal_truncate(ppdb_database_wal_t* wal, uint64_t size) {
    if (!wal) {
        return PPDB_ERR_PARAM;
    }

    ppdb_base_mutex_lock(&wal->mutex);
    if (ftruncate(wal->fd, size) != 0) {
        ppdb_base_mutex_unlock(&wal->mutex);
        return PPDB_ERR_IO;
    }

    wal->size = size;
    ppdb_base_mutex_unlock(&wal->mutex);
    return PPDB_OK;
}

void ppdb_database_wal_destroy(ppdb_database_wal_t* wal) {
    if (!wal) {
        return;
    }

    database_wal_cleanup(wal);
    free(wal);
} 