//-----------------------------------------------------------------------------
// Table Management Implementation
//-----------------------------------------------------------------------------

// Forward declarations from storage_ops.inc.c
static ppdb_error_t begin_write_transaction(ppdb_storage_t* storage);
static ppdb_error_t commit_transaction(ppdb_storage_t* storage);
static ppdb_error_t rollback_transaction(ppdb_storage_t* storage);

ppdb_error_t ppdb_storage_create_table(ppdb_storage_t* storage, const void* name_key, ppdb_storage_table_t** table) {
    if (!storage || !name_key || !table) return PPDB_STORAGE_ERR_PARAM;
    *table = NULL;

    // Create new table
    ppdb_storage_table_t* new_table = malloc(sizeof(ppdb_storage_table_t));
    if (!new_table) return PPDB_STORAGE_ERR_MEMORY;

    // Initialize table structure
    memset(new_table, 0, sizeof(ppdb_storage_table_t));
    new_table->storage = storage;
    new_table->name = strdup((const char*)name_key);
    if (!new_table->name) {
        free(new_table);
        return PPDB_STORAGE_ERR_MEMORY;
    }
    new_table->name_len = strlen(new_table->name);

    // Create engine table
    ppdb_error_t err = ppdb_engine_table_create(storage->current_tx, new_table->name, &new_table->engine_table);
    if (err != PPDB_OK) {
        free(new_table->name);
        free(new_table);
        return err;
    }

    *table = new_table;
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_drop_table(ppdb_storage_t* storage, const void* name_key) {
    if (!storage || !name_key) return PPDB_STORAGE_ERR_PARAM;

    // Find table
    ppdb_storage_table_t* table = NULL;
    ppdb_error_t err = ppdb_storage_get_table(storage, name_key, &table);
    if (err != PPDB_OK) return err;

    // Drop engine table
    err = ppdb_engine_table_drop(storage->current_tx, table->name);
    if (err != PPDB_OK) return err;

    // Cleanup table structure
    ppdb_storage_table_destroy(table);

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const void* name_key, ppdb_storage_table_t** table) {
    if (!storage || !name_key || !table) return PPDB_STORAGE_ERR_PARAM;
    *table = NULL;

    // Create new table structure
    ppdb_storage_table_t* new_table = malloc(sizeof(ppdb_storage_table_t));
    if (!new_table) return PPDB_STORAGE_ERR_MEMORY;

    // Initialize table structure
    memset(new_table, 0, sizeof(ppdb_storage_table_t));
    new_table->storage = storage;
    new_table->name = strdup((const char*)name_key);
    if (!new_table->name) {
        free(new_table);
        return PPDB_STORAGE_ERR_MEMORY;
    }
    new_table->name_len = strlen(new_table->name);

    // Open engine table
    ppdb_error_t err = ppdb_engine_table_open(storage->current_tx, new_table->name, &new_table->engine_table);
    if (err != PPDB_OK) {
        free(new_table->name);
        free(new_table);
        return err;
    }

    *table = new_table;
    return PPDB_OK;
}

void ppdb_storage_table_destroy(ppdb_storage_table_t* table) {
    if (!table) return;

    // 获取存储层锁
    if (table->storage && table->storage->lock) {
        ppdb_base_mutex_lock(table->storage->lock);
    }

    // 回滚活动事务
    if (table->storage && table->storage->current_tx) {
        if (table->storage->current_tx->stats.is_active) {
            ppdb_error_t err = rollback_transaction(table->storage);
            if (err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction during table destroy (code: %d)\n", err);
            }
        }
        table->storage->current_tx = NULL;
    }

    // 获取表锁
    if (table->engine_table && table->engine_table->lock) {
        ppdb_base_mutex_lock(table->engine_table->lock);
    }

    // 释放表资源
    if (table->name) {
        free(table->name);
        table->name = NULL;
    }

    // 关闭引擎表
    if (table->engine_table) {
        ppdb_engine_table_close(table->engine_table);
        table->engine_table = NULL;
    }

    // 释放表锁
    if (table->engine_table && table->engine_table->lock) {
        ppdb_base_mutex_unlock(table->engine_table->lock);
    }

    // 释放存储层锁
    if (table->storage && table->storage->lock) {
        ppdb_base_mutex_unlock(table->storage->lock);
    }

    // 释放表结构
    free(table);
}
