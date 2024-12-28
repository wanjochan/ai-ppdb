#include <cosmopolitan.h>
#include "ppdb/kvstore.h"
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"
#include "ppdb/fs.h"

struct ppdb_kvstore_t {
    char db_path[MAX_PATH_LENGTH];
    struct ppdb_memtable_t* table;
    struct ppdb_wal_t* wal;
    pthread_mutex_t mutex;
    ppdb_mode_t mode;
};

static inline void lock_if_needed(ppdb_kvstore_t* store) {
    if (store->mode == PPDB_MODE_LOCKED) pthread_mutex_lock(&store->mutex);
}

static inline void unlock_if_needed(ppdb_kvstore_t* store) {
    if (store->mode == PPDB_MODE_LOCKED) pthread_mutex_unlock(&store->mutex);
}

static ppdb_error_t create_memtable(ppdb_mode_t mode, size_t size, struct ppdb_memtable_t** table) {
    return mode == PPDB_MODE_LOCKFREE ? 
           ppdb_memtable_create_lockfree(size, table) : 
           ppdb_memtable_create(size, table);
}

static ppdb_error_t create_wal(ppdb_mode_t mode, ppdb_wal_config_t* config, struct ppdb_wal_t** wal) {
    return mode == PPDB_MODE_LOCKFREE ? 
           ppdb_wal_create_lockfree(config, wal) : 
           ppdb_wal_create(config, wal);
}

ppdb_error_t ppdb_kvstore_create(const ppdb_kvstore_config_t* config, ppdb_kvstore_t** store) {
    if (!config || !store) {
        ppdb_log_error("Invalid arguments: config=%p, store=%p", config, store);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Creating KVStore at: %s (mode: %s)", 
                  config->dir_path,
                  config->mode == PPDB_MODE_LOCKFREE ? "lock-free" : "locked");

    ppdb_kvstore_t* new_store = (ppdb_kvstore_t*)calloc(1, sizeof(ppdb_kvstore_t));
    if (!new_store) {
        ppdb_log_error("Failed to allocate KVStore");
        return PPDB_ERR_NO_MEMORY;
    }

    new_store->mode = config->mode;
    size_t path_len = strlen(config->dir_path);
    if (path_len >= MAX_PATH_LENGTH) return PPDB_ERR_PATH_TOO_LONG;
    memcpy(new_store->db_path, config->dir_path, path_len + 1);

    ppdb_error_t err = create_memtable(config->mode, config->memtable_size, &new_store->table);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create MemTable: %d", err);
        free(new_store);
        return err;
    }

    if (config->mode == PPDB_MODE_LOCKED && pthread_mutex_init(&new_store->mutex, NULL) != 0) {
        ppdb_log_error("Failed to initialize mutex");
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return PPDB_ERR_MUTEX_ERROR;
    }

    char wal_path[MAX_PATH_LENGTH];
    if (snprintf(wal_path, sizeof(wal_path), "%s.wal", config->dir_path) >= (int)sizeof(wal_path)) {
        if (config->mode == PPDB_MODE_LOCKED) pthread_mutex_destroy(&new_store->mutex);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    ppdb_wal_config_t wal_config = {
        .segment_size = config->l0_size,
        .sync_write = true,
        .mode = config->mode
    };
    memcpy(wal_config.dir_path, wal_path, sizeof(wal_path));

    err = create_wal(config->mode, &wal_config, &new_store->wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL: %d", err);
        if (config->mode == PPDB_MODE_LOCKED) pthread_mutex_destroy(&new_store->mutex);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    lock_if_needed(new_store);
    err = ppdb_wal_recover(new_store->wal, &new_store->table);
    unlock_if_needed(new_store);

    if (err != PPDB_OK) {
        ppdb_log_error("Failed to recover from WAL: %d", err);
        if (config->mode == PPDB_MODE_LOCKED) pthread_mutex_destroy(&new_store->mutex);
        ppdb_wal_destroy(new_store->wal);
        ppdb_memtable_destroy(new_store->table);
        free(new_store);
        return err;
    }

    *store = new_store;
    ppdb_log_info("Successfully created KVStore at: %s", config->dir_path);
    return PPDB_OK;
}

static void cleanup_store(ppdb_kvstore_t* store, bool destroy) {
    if (!store) return;
    ppdb_log_info("%s KVStore at: %s", destroy ? "Destroying" : "Closing", store->db_path);

    lock_if_needed(store);
    if (store->wal) {
        if (destroy) {
            store->mode == PPDB_MODE_LOCKFREE ? 
                ppdb_wal_destroy_lockfree(store->wal) : 
                ppdb_wal_destroy(store->wal);
        } else {
            store->mode == PPDB_MODE_LOCKFREE ? 
                ppdb_wal_close_lockfree(store->wal) : 
                ppdb_wal_close(store->wal);
        }
        store->wal = NULL;
    }

    if (store->table) {
        store->mode == PPDB_MODE_LOCKFREE ? 
            ppdb_memtable_destroy_lockfree(store->table) : 
            ppdb_memtable_destroy(store->table);
        store->table = NULL;
    }

    unlock_if_needed(store);
    if (store->mode == PPDB_MODE_LOCKED) pthread_mutex_destroy(&store->mutex);
    free(store);
}

void ppdb_kvstore_close(ppdb_kvstore_t* store) {
    cleanup_store(store, false);
}

void ppdb_kvstore_destroy(ppdb_kvstore_t* store) {
    cleanup_store(store, true);
}

static ppdb_error_t handle_memtable_full(ppdb_kvstore_t* store, 
                                        const uint8_t* key, size_t key_len,
                                        const uint8_t* value, size_t value_len) {
    ppdb_memtable_t* new_table = NULL;
    size_t size_limit = store->mode == PPDB_MODE_LOCKFREE ? 
                       ppdb_memtable_max_size_lockfree(store->table) : 
                       ppdb_memtable_max_size(store->table);

    ppdb_error_t err = create_memtable(store->mode, size_limit, &new_table);
    if (err != PPDB_OK) return err;

    ppdb_memtable_destroy(store->table);
    store->table = new_table;
    return store->mode == PPDB_MODE_LOCKFREE ? 
           ppdb_memtable_put_lockfree(store->table, key, key_len, value, value_len) :
           ppdb_memtable_put(store->table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             const uint8_t* value, size_t value_len) {
    if (!store || !key || !value || key_len == 0 || value_len == 0) 
        return PPDB_ERR_NULL_POINTER;

    lock_if_needed(store);
    ppdb_error_t err = store->mode == PPDB_MODE_LOCKFREE ?
        ppdb_wal_write_lockfree(store->wal, PPDB_WAL_RECORD_PUT, key, key_len, value, value_len) :
        ppdb_wal_write(store->wal, PPDB_WAL_RECORD_PUT, key, key_len, value, value_len);

    if (err == PPDB_OK) {
        err = store->mode == PPDB_MODE_LOCKFREE ?
            ppdb_memtable_put_lockfree(store->table, key, key_len, value, value_len) :
            ppdb_memtable_put(store->table, key, key_len, value, value_len);

        if (err == PPDB_ERR_FULL) {
            err = handle_memtable_full(store, key, key_len, value, value_len);
        }
    }

    unlock_if_needed(store);
    return err;
}

ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* store,
                             const uint8_t* key, size_t key_len,
                             uint8_t* value, size_t* value_len) {
    if (!store || !key || !value_len) return PPDB_ERR_NULL_POINTER;

    if (store->mode == PPDB_MODE_LOCKED) {
        pthread_mutex_lock(&store->mutex);
        ppdb_error_t ret = ppdb_memtable_get(store->table, key, key_len, value, value_len);
        pthread_mutex_unlock(&store->mutex);
        return ret;
    } else {
        return ppdb_memtable_get_lockfree(store->table, key, key_len, value, value_len);
    }
}

ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store,
                                const uint8_t* key, size_t key_len) {
    if (!store || !key || key_len == 0) 
        return PPDB_ERR_NULL_POINTER;

    lock_if_needed(store);
    ppdb_error_t err = store->mode == PPDB_MODE_LOCKFREE ?
        ppdb_wal_write_lockfree(store->wal, PPDB_WAL_RECORD_DELETE, key, key_len, NULL, 0) :
        ppdb_wal_write(store->wal, PPDB_WAL_RECORD_DELETE, key, key_len, NULL, 0);

    if (err == PPDB_OK) {
        err = store->mode == PPDB_MODE_LOCKFREE ?
            ppdb_memtable_delete_lockfree(store->table, key, key_len) :
            ppdb_memtable_delete(store->table, key, key_len);
    }

    unlock_if_needed(store);
    return err;
}