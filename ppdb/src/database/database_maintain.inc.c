#include "internal/database.h"
#include "internal/base.h"

static int database_maintain_check_integrity(ppdb_database_t* db) {
    if (!db) {
        return PPDB_ERR_PARAM;
    }

    // Check database components
    if (!db->txn_manager || !db->table_manager || !db->index_manager) {
        return PPDB_DATABASE_ERR_CORRUPT;
    }

    // Check tables
    ppdb_database_table_t* tables = NULL;
    size_t table_count = 0;
    int ret = database_table_manager_get_tables(db->table_manager, &tables, &table_count);
    if (ret != PPDB_OK) {
        return ret;
    }

    for (size_t i = 0; i < table_count; i++) {
        if (!tables[i].schema || !tables[i].primary_index) {
            free(tables);
            return PPDB_DATABASE_ERR_CORRUPT;
        }
    }

    free(tables);
    return PPDB_OK;
}

int ppdb_database_maintain_compact(ppdb_database_t* db) {
    if (!db) {
        return PPDB_ERR_PARAM;
    }

    // Check database integrity first
    int ret = database_maintain_check_integrity(db);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Lock database for maintenance
    pthread_rwlock_wrlock(&db->rwlock);

    // Compact tables
    ppdb_database_table_t* tables = NULL;
    size_t table_count = 0;
    ret = database_table_manager_get_tables(db->table_manager, &tables, &table_count);
    if (ret != PPDB_OK) {
        pthread_rwlock_unlock(&db->rwlock);
        return ret;
    }

    for (size_t i = 0; i < table_count; i++) {
        // Compact table data
        ret = database_table_compact(&tables[i]);
        if (ret != PPDB_OK) {
            free(tables);
            pthread_rwlock_unlock(&db->rwlock);
            return ret;
        }

        // Rebuild indexes
        ret = database_table_rebuild_indexes(&tables[i]);
        if (ret != PPDB_OK) {
            free(tables);
            pthread_rwlock_unlock(&db->rwlock);
            return ret;
        }
    }

    free(tables);
    pthread_rwlock_unlock(&db->rwlock);
    return PPDB_OK;
}

int ppdb_database_maintain_verify(ppdb_database_t* db) {
    if (!db) {
        return PPDB_ERR_PARAM;
    }

    // Check database integrity
    int ret = database_maintain_check_integrity(db);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Lock database for verification
    pthread_rwlock_rdlock(&db->rwlock);

    // Verify tables
    ppdb_database_table_t* tables = NULL;
    size_t table_count = 0;
    ret = database_table_manager_get_tables(db->table_manager, &tables, &table_count);
    if (ret != PPDB_OK) {
        pthread_rwlock_unlock(&db->rwlock);
        return ret;
    }

    for (size_t i = 0; i < table_count; i++) {
        // Verify table data
        ret = database_table_verify(&tables[i]);
        if (ret != PPDB_OK) {
            free(tables);
            pthread_rwlock_unlock(&db->rwlock);
            return ret;
        }

        // Verify indexes
        ret = database_table_verify_indexes(&tables[i]);
        if (ret != PPDB_OK) {
            free(tables);
            pthread_rwlock_unlock(&db->rwlock);
            return ret;
        }
    }

    free(tables);
    pthread_rwlock_unlock(&db->rwlock);
    return PPDB_OK;
}

int ppdb_database_maintain_backup(ppdb_database_t* db, const char* backup_dir) {
    if (!db || !backup_dir) {
        return PPDB_ERR_PARAM;
    }

    // Check database integrity first
    int ret = database_maintain_check_integrity(db);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Lock database for backup
    pthread_rwlock_rdlock(&db->rwlock);

    // Create backup directory
    if (mkdir(backup_dir, 0755) != 0) {
        pthread_rwlock_unlock(&db->rwlock);
        return PPDB_ERR_IO;
    }

    // Backup tables
    ppdb_database_table_t* tables = NULL;
    size_t table_count = 0;
    ret = database_table_manager_get_tables(db->table_manager, &tables, &table_count);
    if (ret != PPDB_OK) {
        pthread_rwlock_unlock(&db->rwlock);
        return ret;
    }

    for (size_t i = 0; i < table_count; i++) {
        char table_backup_path[PPDB_MAX_PATH_LEN];
        snprintf(table_backup_path, sizeof(table_backup_path), "%s/%s",
                backup_dir, tables[i].name);

        ret = database_table_backup(&tables[i], table_backup_path);
        if (ret != PPDB_OK) {
            free(tables);
            pthread_rwlock_unlock(&db->rwlock);
            return ret;
        }
    }

    free(tables);
    pthread_rwlock_unlock(&db->rwlock);
    return PPDB_OK;
} 