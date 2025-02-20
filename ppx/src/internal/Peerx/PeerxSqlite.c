#include "PeerxSqlite.h"
#include "sqlite3/sqlite3.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
// #include <string.h>
// #include <stdio.h>

// Error codes
#define INFRAX_ERROR_IO -10
#define INFRAX_ERROR_SQL INFRAX_ERROR_IO
#define INFRAX_ERROR_NOT_FOUND INFRAX_ERROR_FILE_NOT_FOUND

// Forward declarations
static const char* polyx_service_config_get_string(const polyx_service_config_t* config, const char* key, const char* default_value);

// Private data structure
typedef struct {
    InfraxMemory* memory;
    InfraxCore* core;
    sqlite3* db;
    peerx_sqlite_conn_info_t conn_info;
    bool initialized;
    bool in_transaction;
} PeerxSqlitePrivate;

// Global memory manager and core
static InfraxMemory* g_memory = NULL;
static InfraxCore* g_core = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxCoreClassType InfraxCoreClass;

// Forward declarations of private functions
static bool init_memory(void);
static void free_result_internal(peerx_sqlite_result_t* result);
static InfraxError get_config_string(const polyx_service_config_t* config, const char* key, const char** value);

// Forward declarations
static InfraxError peerx_sqlite_open(PeerxSqlite* self, const peerx_sqlite_conn_info_t* info);
static InfraxError peerx_sqlite_close(PeerxSqlite* self);
static InfraxError peerx_sqlite_exec(PeerxSqlite* self, const char* sql);
static InfraxError peerx_sqlite_query(PeerxSqlite* self, const char* sql, peerx_sqlite_result_t* result);
static void peerx_sqlite_free_result(PeerxSqlite* self, peerx_sqlite_result_t* result);
static InfraxError peerx_sqlite_begin(PeerxSqlite* self);
static InfraxError peerx_sqlite_commit(PeerxSqlite* self);
static InfraxError peerx_sqlite_rollback(PeerxSqlite* self);
static InfraxError peerx_sqlite_backup(PeerxSqlite* self, const char* path);
static InfraxError peerx_sqlite_restore(PeerxSqlite* self, const char* path);
static InfraxError peerx_sqlite_init(PeerxSqlite* self, const polyx_service_config_t* config);
static InfraxError peerx_sqlite_start(PeerxSqlite* self);
static InfraxError peerx_sqlite_stop(PeerxSqlite* self);
static InfraxError peerx_sqlite_reload(PeerxSqlite* self);
static InfraxError peerx_sqlite_get_status(PeerxSqlite* self, char* status, size_t size);

// Constructor
static PeerxSqlite* peerx_sqlite_new(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Allocate instance
    PeerxSqlite* self = g_memory->alloc(g_memory, sizeof(PeerxSqlite));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    g_core->memset(g_core, self, 0, sizeof(PeerxSqlite));
    
    // Initialize base service
    PeerxService* base = PeerxServiceClass.new();
    if (!base) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    g_core->memcpy(g_core, &self->base, base, sizeof(PeerxService));
    g_memory->dealloc(g_memory, base);

    // Allocate private data
    PeerxSqlitePrivate* private = g_memory->alloc(g_memory, sizeof(PeerxSqlitePrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    g_core->memset(g_core, private, 0, sizeof(PeerxSqlitePrivate));
    private->memory = g_memory;
    private->core = g_core;

    self->base.private_data = private;

    // Initialize function pointers
    self->open = peerx_sqlite_open;
    self->close = peerx_sqlite_close;
    self->exec = peerx_sqlite_exec;
    self->query = peerx_sqlite_query;
    self->free_result = peerx_sqlite_free_result;
    self->begin = peerx_sqlite_begin;
    self->commit = peerx_sqlite_commit;
    self->rollback = peerx_sqlite_rollback;
    self->backup = peerx_sqlite_backup;
    self->restore = peerx_sqlite_restore;

    return self;
}

// Destructor
static void peerx_sqlite_free(PeerxSqlite* self) {
    if (!self) return;

    // Stop service if running
    if (self->base.is_running) {
        self->base.klass->stop(&self->base);
    }

    // Close database if open
    PeerxSqlitePrivate* private = self->base.private_data;
    if (private) {
        if (private->db) {
            sqlite3_close(private->db);
        }
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Database operations
static InfraxError peerx_sqlite_open(PeerxSqlite* self, const peerx_sqlite_conn_info_t* info) {
    if (!self || !info) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Close existing connection
    if (private->db) {
        sqlite3_close(private->db);
        private->db = NULL;
    }

    // Save connection info
    g_core->memcpy(g_core, &private->conn_info, info, sizeof(peerx_sqlite_conn_info_t));

    // Open database
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (info->read_only) {
        flags = SQLITE_OPEN_READONLY;
    }

    const char* path = info->in_memory ? ":memory:" : info->path;
    int rc = sqlite3_open_v2(path, &private->db, flags, NULL);
    if (rc != SQLITE_OK) {
        return make_error(INFRAX_ERROR_IO, sqlite3_errmsg(private->db));
    }

    // Configure connection
    sqlite3_busy_timeout(private->db, info->timeout_ms);

    private->initialized = true;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_sqlite_close(PeerxSqlite* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    if (!private->db) {
        return make_error(INFRAX_ERROR_OK, NULL);
    }

    // Close database
    int rc = sqlite3_close(private->db);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to close database: %s", sqlite3_errmsg(private->db));
        return make_error(INFRAX_ERROR_IO, "Failed to close database");
    }

    private->db = NULL;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_sqlite_exec(PeerxSqlite* self, const char* sql) {
    if (!self || !sql) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    char* errmsg = NULL;
    int rc = sqlite3_exec(private->db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to execute SQL: %s", errmsg);
        sqlite3_free(errmsg);
        return make_error(INFRAX_ERROR_IO, "Failed to execute SQL");
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_sqlite_query(PeerxSqlite* self, const char* sql, peerx_sqlite_result_t* result) {
    if (!self || !sql || !result) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    // Initialize result
    g_core->memset(g_core, result, 0, sizeof(peerx_sqlite_result_t));

    // Prepare statement
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(private->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return make_error(INFRAX_ERROR_SQL, sqlite3_errmsg(private->db));
    }

    // Get column info
    result->column_count = sqlite3_column_count(stmt);
    result->column_names = g_memory->alloc(g_memory, result->column_count * sizeof(char*));
    if (!result->column_names) {
        sqlite3_finalize(stmt);
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate column names");
    }

    for (size_t i = 0; i < result->column_count; i++) {
        const char* name = sqlite3_column_name(stmt, i);
        result->column_names[i] = g_memory->alloc(g_memory, g_core->strlen(g_core, name) + 1);
        if (!result->column_names[i]) {
            free_result_internal(result);
            sqlite3_finalize(stmt);
            return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate column name");
        }
        g_core->strcpy(g_core, result->column_names[i], name);
    }

    // Fetch rows
    size_t capacity = 16;
    result->rows = g_memory->alloc(g_memory, capacity * sizeof(char**));
    if (!result->rows) {
        free_result_internal(result);
        sqlite3_finalize(stmt);
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate rows");
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        // Grow rows array if needed
        if (result->row_count >= capacity) {
            size_t new_capacity = capacity * 2;
            char*** new_rows = g_memory->alloc(g_memory, new_capacity * sizeof(char**));
            if (!new_rows) {
                free_result_internal(result);
                sqlite3_finalize(stmt);
                return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to grow rows array");
            }
            g_core->memcpy(g_core, new_rows, result->rows, capacity * sizeof(char**));
            g_memory->dealloc(g_memory, result->rows);
            result->rows = new_rows;
            capacity = new_capacity;
        }

        // Allocate row
        result->rows[result->row_count] = g_memory->alloc(g_memory, result->column_count * sizeof(char*));
        if (!result->rows[result->row_count]) {
            free_result_internal(result);
            sqlite3_finalize(stmt);
            return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate row");
        }

        // Copy values
        for (size_t i = 0; i < result->column_count; i++) {
            const unsigned char* value = sqlite3_column_text(stmt, i);
            if (value) {
                result->rows[result->row_count][i] = g_memory->alloc(g_memory, g_core->strlen(g_core, (const char*)value) + 1);
                if (!result->rows[result->row_count][i]) {
                    free_result_internal(result);
                    sqlite3_finalize(stmt);
                    return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate cell");
                }
                g_core->strcpy(g_core, result->rows[result->row_count][i], (const char*)value);
            } else {
                result->rows[result->row_count][i] = NULL;
            }
        }

        result->row_count++;
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        free_result_internal(result);
        return make_error(INFRAX_ERROR_SQL, sqlite3_errmsg(private->db));
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

static void peerx_sqlite_free_result(PeerxSqlite* self, peerx_sqlite_result_t* result) {
    if (!self || !result) return;
    free_result_internal(result);
}

// Transaction management
static InfraxError peerx_sqlite_begin(PeerxSqlite* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    if (private->in_transaction) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Transaction already in progress");
    }

    InfraxError err = peerx_sqlite_exec(self, "BEGIN TRANSACTION");
    if (err.code == INFRAX_ERROR_OK) {
        private->in_transaction = true;
    }
    return err;
}

static InfraxError peerx_sqlite_commit(PeerxSqlite* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    if (!private->in_transaction) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "No transaction in progress");
    }

    InfraxError err = peerx_sqlite_exec(self, "COMMIT");
    if (err.code == INFRAX_ERROR_OK) {
        private->in_transaction = false;
    }
    return err;
}

static InfraxError peerx_sqlite_rollback(PeerxSqlite* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    if (!private->in_transaction) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "No transaction in progress");
    }

    InfraxError err = peerx_sqlite_exec(self, "ROLLBACK");
    if (err.code == INFRAX_ERROR_OK) {
        private->in_transaction = false;
    }
    return err;
}

// Backup and restore
static InfraxError peerx_sqlite_backup(PeerxSqlite* self, const char* path) {
    if (!self || !path) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    sqlite3* backup_db = NULL;
    int rc = sqlite3_open(path, &backup_db);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to open backup database: %s", sqlite3_errmsg(backup_db));
        if (backup_db) {
            sqlite3_close(backup_db);
        }
        return make_error(INFRAX_ERROR_IO, "Failed to open backup database");
    }

    sqlite3_backup* backup = sqlite3_backup_init(backup_db, "main", private->db, "main");
    if (!backup) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to initialize backup: %s", sqlite3_errmsg(backup_db));
        sqlite3_close(backup_db);
        return make_error(INFRAX_ERROR_IO, "Failed to initialize backup");
    }

    rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(backup_db);

    if (rc != SQLITE_DONE) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to backup database: %s", sqlite3_errmsg(private->db));
        return make_error(INFRAX_ERROR_IO, "Failed to backup database");
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_sqlite_restore(PeerxSqlite* self, const char* path) {
    if (!self || !path) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    sqlite3* restore_db = NULL;
    int rc = sqlite3_open(path, &restore_db);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to open restore database: %s", sqlite3_errmsg(restore_db));
        if (restore_db) {
            sqlite3_close(restore_db);
        }
        return make_error(INFRAX_ERROR_IO, "Failed to open restore database");
    }

    sqlite3_backup* backup = sqlite3_backup_init(private->db, "main", restore_db, "main");
    if (!backup) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to initialize restore: %s", sqlite3_errmsg(private->db));
        sqlite3_close(restore_db);
        return make_error(INFRAX_ERROR_IO, "Failed to initialize restore");
    }

    rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(restore_db);

    if (rc != SQLITE_DONE) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to restore database: %s", sqlite3_errmsg(private->db));
        return make_error(INFRAX_ERROR_IO, "Failed to restore database");
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Service lifecycle
static InfraxError peerx_sqlite_init(PeerxSqlite* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Initialize base service
    InfraxError err = self->base.klass->init(&self->base, config);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Get database path from config
    const char* db_path = NULL;
    err = get_config_string(config, "db_path", &db_path);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Open database
    peerx_sqlite_conn_info_t conn_info = {0};
    if (db_path) {
        g_core->strncpy(g_core, conn_info.path, db_path, sizeof(conn_info.path) - 1);
    } else {
        conn_info.in_memory = true;
    }
    conn_info.timeout_ms = 5000;

    return peerx_sqlite_open(self, &conn_info);
}

static InfraxError peerx_sqlite_start(PeerxSqlite* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Start base service
    return PeerxServiceClass.start(&self->base);
}

static InfraxError peerx_sqlite_stop(PeerxSqlite* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Stop base service
    InfraxError err = PeerxServiceClass.stop(&self->base);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Close database
    return peerx_sqlite_close(self);
}

static InfraxError peerx_sqlite_reload(PeerxSqlite* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Reload base service
    return PeerxServiceClass.reload(&self->base);
}

// Status and error handling
static InfraxError peerx_sqlite_get_status(PeerxSqlite* self, char* status, size_t size) {
    if (!self || !status || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Get base status
    InfraxError err = self->base.klass->get_status(&self->base, status, size);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Add SQLite specific status
    const char* conn_state = private->initialized ? "connected" : "not connected";
    const char* tx_state = private->in_transaction ? "in transaction" : "no transaction";
    g_core->snprintf(g_core, status, size, "%s\nState: %s, %s", status, conn_state, tx_state);

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Private helper functions
static bool init_memory(void) {
    if (g_memory) return true;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    return g_memory != NULL;
}

static void free_result_internal(peerx_sqlite_result_t* result) {
    if (!result || !g_memory || !g_core) return;

    if (result->column_names) {
        for (size_t i = 0; i < result->column_count; i++) {
            if (result->column_names[i]) {
                g_memory->dealloc(g_memory, result->column_names[i]);
            }
        }
        g_memory->dealloc(g_memory, result->column_names);
    }

    if (result->rows) {
        for (size_t i = 0; i < result->row_count; i++) {
            if (result->rows[i]) {
                for (size_t j = 0; j < result->column_count; j++) {
                    if (result->rows[i][j]) {
                        g_memory->dealloc(g_memory, result->rows[i][j]);
                    }
                }
                g_memory->dealloc(g_memory, result->rows[i]);
            }
        }
        g_memory->dealloc(g_memory, result->rows);
    }

    g_core->memset(g_core, result, 0, sizeof(peerx_sqlite_result_t));
}

// Global class instance
const PeerxSqliteClassType PeerxSqliteClass = {
    .new = peerx_sqlite_new,
    .free = peerx_sqlite_free,
    .init = (InfraxError (*)(PeerxSqlite*, const polyx_service_config_t*))peerx_sqlite_init,
    .start = (InfraxError (*)(PeerxSqlite*))peerx_sqlite_start,
    .stop = (InfraxError (*)(PeerxSqlite*))peerx_sqlite_stop,
    .reload = (InfraxError (*)(PeerxSqlite*))peerx_sqlite_reload,
    .get_status = (InfraxError (*)(PeerxSqlite*, char*, size_t))peerx_sqlite_get_status,
    .get_error = (const char* (*)(PeerxSqlite*))PeerxServiceClass.get_error,
    .clear_error = (void (*)(PeerxSqlite*))PeerxServiceClass.clear_error,
    .validate_config = (InfraxError (*)(PeerxSqlite*, const polyx_service_config_t*))PeerxServiceClass.validate_config,
    .apply_config = (InfraxError (*)(PeerxSqlite*, const polyx_service_config_t*))PeerxServiceClass.apply_config
};

// Helper function for config
static InfraxError get_config_string(const polyx_service_config_t* config, const char* key, const char** value) {
    if (!config || !key || !value) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    if (g_core->strcmp(g_core, key, "db_path") == 0) {
        *value = config->backend;  // Use backend field for db_path
        return make_error(INFRAX_ERROR_OK, NULL);
    }

    return make_error(INFRAX_ERROR_NOT_FOUND, "Key not found");
} 