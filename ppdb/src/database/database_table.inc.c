#include "internal/database.h"
#include "internal/base.h"

static int database_table_init_internal(ppdb_database_table_t* table, const char* name) {
    if (!table || !name) {
        return PPDB_ERR_PARAM;
    }

    strncpy(table->name, name, PPDB_MAX_NAME_LEN);
    table->name[PPDB_MAX_NAME_LEN - 1] = '\0';
    
    table->record_count = 0;
    table->stats = (ppdb_database_table_stats_t){0};

    // Initialize table locks
    ppdb_base_mutex_create(&table->mutex, NULL);
    ppdb_base_rwlock_create(&table->rwlock, NULL);

    // Initialize table components
    table->primary_index = NULL;
    table->secondary_indexes = NULL;
    table->schema = NULL;

    return PPDB_OK;
}

static void database_table_cleanup_internal(ppdb_database_table_t* table) {
    if (!table) {
        return;
    }

    // Cleanup table components
    if (table->primary_index) {
        database_index_destroy(table->primary_index);
    }
    if (table->secondary_indexes) {
        database_index_list_destroy(table->secondary_indexes);
    }
    if (table->schema) {
        database_schema_destroy(table->schema);
    }

    // Cleanup locks
    ppdb_base_mutex_destroy(&table->mutex);
    ppdb_base_rwlock_destroy(&table->rwlock);
}

int ppdb_database_table_create(ppdb_database_t* db, const char* name, ppdb_database_table_t** table) {
    if (!db || !name || !table) {
        return PPDB_ERR_PARAM;
    }

    *table = (ppdb_database_table_t*)calloc(1, sizeof(ppdb_database_table_t));
    if (!*table) {
        return PPDB_ERR_NOMEM;
    }

    int ret = database_table_init_internal(*table, name);
    if (ret != PPDB_OK) {
        free(*table);
        *table = NULL;
        return ret;
    }

    // Add table to database
    ret = database_table_manager_add_table(db->table_manager, *table);
    if (ret != PPDB_OK) {
        database_table_cleanup_internal(*table);
        free(*table);
        *table = NULL;
        return ret;
    }

    return PPDB_OK;
}

int ppdb_database_table_drop(ppdb_database_t* db, const char* name) {
    if (!db || !name) {
        return PPDB_ERR_PARAM;
    }

    ppdb_database_table_t* table = NULL;
    int ret = database_table_manager_get_table(db->table_manager, name, &table);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Remove table from database
    ret = database_table_manager_remove_table(db->table_manager, name);
    if (ret != PPDB_OK) {
        return ret;
    }

    database_table_cleanup_internal(table);
    free(table);

    return PPDB_OK;
}

void ppdb_database_table_destroy(ppdb_database_table_t* table) {
    if (!table) {
        return;
    }

    database_table_cleanup_internal(table);
    free(table);
} 