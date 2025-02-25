#include "PeerxSqlite.h"
#include "internal/infrax/InfraxMemory.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>

// Private data structure
typedef struct {
    InfraxMemory* memory;
    sqlite3* db;
    peerx_sqlite_conn_info_t conn_info;
    bool in_transaction;
} PeerxSqlitePrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;

// Forward declarations of private functions
static bool init_memory(void);
static void free_result_internal(peerx_sqlite_result_t* result);

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
    memset(self, 0, sizeof(PeerxSqlite));
    
    // Initialize base service
    PeerxService* base = PeerxServiceClass.new();
    if (!base) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    memcpy(&self->base, base, sizeof(PeerxService));
    g_memory->dealloc(g_memory, base);

    // Allocate private data
    PeerxSqlitePrivate* private = g_memory->alloc(g_memory, sizeof(PeerxSqlitePrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    memset(private, 0, sizeof(PeerxSqlitePrivate));
    private->memory = g_memory;

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
static infrax_error_t peerx_sqlite_open(PeerxSqlite* self, const peerx_sqlite_conn_info_t* info) {
    if (!self || !info) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Close existing connection
    if (private->db) {
        sqlite3_close(private->db);
        private->db = NULL;
    }

    // Open database
    int flags = info->read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    const char* path = info->in_memory ? ":memory:" : info->path;
    
    int rc = sqlite3_open_v2(path, &private->db, flags, NULL);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to open database: %s", sqlite3_errmsg(private->db));
        return INFRAX_ERROR_IO;
    }

    // Set busy timeout
    if (info->timeout_ms > 0) {
        sqlite3_busy_timeout(private->db, info->timeout_ms);
    }

    // Save connection info
    memcpy(&private->conn_info, info, sizeof(peerx_sqlite_conn_info_t));
    return INFRAX_OK;
}

static infrax_error_t peerx_sqlite_close(PeerxSqlite* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    if (!private->db) {
        return INFRAX_OK;
    }

    // Close database
    int rc = sqlite3_close(private->db);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to close database: %s", sqlite3_errmsg(private->db));
        return INFRAX_ERROR_IO;
    }

    private->db = NULL;
    return INFRAX_OK;
}

static infrax_error_t peerx_sqlite_exec(PeerxSqlite* self, const char* sql) {
    if (!self || !sql) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    char* errmsg = NULL;
    int rc = sqlite3_exec(private->db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to execute SQL: %s", errmsg);
        sqlite3_free(errmsg);
        return INFRAX_ERROR_IO;
    }

    return INFRAX_OK;
}

static infrax_error_t peerx_sqlite_query(PeerxSqlite* self, const char* sql, 
                                       peerx_sqlite_result_t* result) {
    if (!self || !sql || !result) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(private->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to prepare SQL: %s", sqlite3_errmsg(private->db));
        return INFRAX_ERROR_IO;
    }

    // Get column count and names
    result->column_count = sqlite3_column_count(stmt);
    result->column_names = g_memory->alloc(g_memory, result->column_count * sizeof(char*));
    if (!result->column_names) {
        sqlite3_finalize(stmt);
        return INFRAX_ERROR_NO_MEMORY;
    }

    for (int i = 0; i < result->column_count; i++) {
        const char* name = sqlite3_column_name(stmt, i);
        result->column_names[i] = g_memory->alloc(g_memory, strlen(name) + 1);
        if (!result->column_names[i]) {
            free_result_internal(result);
            sqlite3_finalize(stmt);
            return INFRAX_ERROR_NO_MEMORY;
        }
        strcpy(result->column_names[i], name);
    }

    // Allocate initial rows array
    size_t capacity = 16;
    result->rows = g_memory->alloc(g_memory, capacity * sizeof(char**));
    if (!result->rows) {
        free_result_internal(result);
        sqlite3_finalize(stmt);
        return INFRAX_ERROR_NO_MEMORY;
    }

    // Fetch rows
    result->row_count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        // Expand rows array if needed
        if (result->row_count >= capacity) {
            size_t new_capacity = capacity * 2;
            char*** new_rows = g_memory->alloc(g_memory, new_capacity * sizeof(char**));
            if (!new_rows) {
                free_result_internal(result);
                sqlite3_finalize(stmt);
                return INFRAX_ERROR_NO_MEMORY;
            }
            memcpy(new_rows, result->rows, result->row_count * sizeof(char**));
            g_memory->dealloc(g_memory, result->rows);
            result->rows = new_rows;
            capacity = new_capacity;
        }

        // Allocate row
        char** row = g_memory->alloc(g_memory, result->column_count * sizeof(char*));
        if (!row) {
            free_result_internal(result);
            sqlite3_finalize(stmt);
            return INFRAX_ERROR_NO_MEMORY;
        }

        // Copy column values
        for (int i = 0; i < result->column_count; i++) {
            const char* value = (const char*)sqlite3_column_text(stmt, i);
            if (value) {
                row[i] = g_memory->alloc(g_memory, strlen(value) + 1);
                if (!row[i]) {
                    for (int j = 0; j < i; j++) {
                        g_memory->dealloc(g_memory, row[j]);
                    }
                    g_memory->dealloc(g_memory, row);
                    free_result_internal(result);
                    sqlite3_finalize(stmt);
                    return INFRAX_ERROR_NO_MEMORY;
                }
                strcpy(row[i], value);
            } else {
                row[i] = NULL;
            }
        }

        result->rows[result->row_count++] = row;
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to fetch results: %s", sqlite3_errmsg(private->db));
        free_result_internal(result);
        return INFRAX_ERROR_IO;
    }

    return INFRAX_OK;
}

static void peerx_sqlite_free_result(PeerxSqlite* self, peerx_sqlite_result_t* result) {
    if (!self || !result) return;
    free_result_internal(result);
}

// Transaction management
static infrax_error_t peerx_sqlite_begin(PeerxSqlite* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    if (private->in_transaction) {
        PEERX_SERVICE_ERROR(&self->base, "Transaction already in progress");
        return INFRAX_ERROR_INVALID_STATE;
    }

    infrax_error_t err = peerx_sqlite_exec(self, "BEGIN TRANSACTION");
    if (err == INFRAX_OK) {
        private->in_transaction = true;
    }
    return err;
}

static infrax_error_t peerx_sqlite_commit(PeerxSqlite* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    if (!private->in_transaction) {
        PEERX_SERVICE_ERROR(&self->base, "No transaction in progress");
        return INFRAX_ERROR_INVALID_STATE;
    }

    infrax_error_t err = peerx_sqlite_exec(self, "COMMIT");
    if (err == INFRAX_OK) {
        private->in_transaction = false;
    }
    return err;
}

static infrax_error_t peerx_sqlite_rollback(PeerxSqlite* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    if (!private->in_transaction) {
        PEERX_SERVICE_ERROR(&self->base, "No transaction in progress");
        return INFRAX_ERROR_INVALID_STATE;
    }

    infrax_error_t err = peerx_sqlite_exec(self, "ROLLBACK");
    if (err == INFRAX_OK) {
        private->in_transaction = false;
    }
    return err;
}

// Backup and restore
static infrax_error_t peerx_sqlite_backup(PeerxSqlite* self, const char* path) {
    if (!self || !path) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Open backup database
    sqlite3* backup_db = NULL;
    int rc = sqlite3_open(path, &backup_db);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to open backup database: %s", 
                          sqlite3_errmsg(backup_db));
        sqlite3_close(backup_db);
        return INFRAX_ERROR_IO;
    }

    // Create backup
    sqlite3_backup* backup = sqlite3_backup_init(backup_db, "main", private->db, "main");
    if (!backup) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to initialize backup: %s", 
                          sqlite3_errmsg(backup_db));
        sqlite3_close(backup_db);
        return INFRAX_ERROR_IO;
    }

    rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);

    if (rc != SQLITE_DONE) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to perform backup: %s", 
                          sqlite3_errmsg(backup_db));
        sqlite3_close(backup_db);
        return INFRAX_ERROR_IO;
    }

    sqlite3_close(backup_db);
    return INFRAX_OK;
}

static infrax_error_t peerx_sqlite_restore(PeerxSqlite* self, const char* path) {
    if (!self || !path) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Open source database
    sqlite3* source_db = NULL;
    int rc = sqlite3_open(path, &source_db);
    if (rc != SQLITE_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to open source database: %s", 
                          sqlite3_errmsg(source_db));
        sqlite3_close(source_db);
        return INFRAX_ERROR_IO;
    }

    // Create backup
    sqlite3_backup* backup = sqlite3_backup_init(private->db, "main", source_db, "main");
    if (!backup) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to initialize restore: %s", 
                          sqlite3_errmsg(private->db));
        sqlite3_close(source_db);
        return INFRAX_ERROR_IO;
    }

    rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);

    if (rc != SQLITE_DONE) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to perform restore: %s", 
                          sqlite3_errmsg(private->db));
        sqlite3_close(source_db);
        return INFRAX_ERROR_IO;
    }

    sqlite3_close(source_db);
    return INFRAX_OK;
}

// Service lifecycle
static infrax_error_t peerx_sqlite_init(PeerxSqlite* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Initialize base service
    infrax_error_t err = PeerxServiceClass.init(&self->base, config);
    if (err != INFRAX_OK) {
        return err;
    }

    // Open database from config
    peerx_sqlite_conn_info_t conn_info = {
        .read_only = false,
        .in_memory = false,
        .timeout_ms = 5000
    };
    strncpy(conn_info.path, config->backend, sizeof(conn_info.path) - 1);

    return peerx_sqlite_open(self, &conn_info);
}

static infrax_error_t peerx_sqlite_start(PeerxSqlite* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Start base service
    return PeerxServiceClass.start(&self->base);
}

static infrax_error_t peerx_sqlite_stop(PeerxSqlite* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Stop base service
    infrax_error_t err = PeerxServiceClass.stop(&self->base);
    if (err != INFRAX_OK) {
        return err;
    }

    // Close database
    return peerx_sqlite_close(self);
}

static infrax_error_t peerx_sqlite_reload(PeerxSqlite* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Reload base service
    return PeerxServiceClass.reload(&self->base);
}

// Status and error handling
static infrax_error_t peerx_sqlite_get_status(PeerxSqlite* self, char* status, size_t size) {
    if (!self || !status || size == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxSqlitePrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Get base status
    char base_status[512];
    infrax_error_t err = PeerxServiceClass.get_status(&self->base, base_status, sizeof(base_status));
    if (err != INFRAX_OK) {
        return err;
    }

    // Add SQLite specific status
    snprintf(status, size, "%s\nDatabase: %s, Transaction: %s",
             base_status,
             private->db ? (private->conn_info.in_memory ? "memory" : private->conn_info.path) : "closed",
             private->in_transaction ? "in progress" : "none");

    return INFRAX_OK;
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
    if (!result) return;

    if (result->column_names) {
        for (int i = 0; i < result->column_count; i++) {
            if (result->column_names[i]) {
                g_memory->dealloc(g_memory, result->column_names[i]);
            }
        }
        g_memory->dealloc(g_memory, result->column_names);
    }

    if (result->rows) {
        for (int i = 0; i < result->row_count; i++) {
            if (result->rows[i]) {
                for (int j = 0; j < result->column_count; j++) {
                    if (result->rows[i][j]) {
                        g_memory->dealloc(g_memory, result->rows[i][j]);
                    }
                }
                g_memory->dealloc(g_memory, result->rows[i]);
            }
        }
        g_memory->dealloc(g_memory, result->rows);
    }

    memset(result, 0, sizeof(peerx_sqlite_result_t));
}

// Global class instance
const PeerxSqliteClassType PeerxSqliteClass = {
    .new = peerx_sqlite_new,
    .free = peerx_sqlite_free,
    .init = peerx_sqlite_init,
    .start = peerx_sqlite_start,
    .stop = peerx_sqlite_stop,
    .reload = peerx_sqlite_reload,
    .get_status = peerx_sqlite_get_status,
    .get_error = PeerxServiceClass.get_error,
    .clear_error = PeerxServiceClass.clear_error,
    .validate_config = PeerxServiceClass.validate_config,
    .apply_config = PeerxServiceClass.apply_config
}; 