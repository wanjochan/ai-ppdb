#include "PolyxDB.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/peerx/PeerxSqlite.h"
#include <string.h>
#include <stdio.h>

// Private data structure
typedef struct {
    InfraxMemory* memory;
    union {
        PeerxSqlite* sqlite;
        // TODO: Add DuckDB support
    } db;
    polyx_db_config_t config;
    bool initialized;
    char* error_message;
} PolyxDBPrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern PeerxSqliteClassType PeerxSqliteClass;

// Forward declarations of private functions
static bool init_memory(void);
static void free_result_internal(polyx_db_result_t* result);
static void set_error(PolyxDB* self, const char* format, ...);

// Constructor
static PolyxDB* polyx_db_new(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Allocate instance
    PolyxDB* self = g_memory->alloc(g_memory, sizeof(PolyxDB));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    memset(self, 0, sizeof(PolyxDB));
    self->self = self;
    self->klass = &PolyxDBClass;

    // Allocate private data
    PolyxDBPrivate* private = g_memory->alloc(g_memory, sizeof(PolyxDBPrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    memset(private, 0, sizeof(PolyxDBPrivate));
    private->memory = g_memory;
    self->private_data = private;

    return self;
}

// Destructor
static void polyx_db_free(PolyxDB* self) {
    if (!self) return;

    PolyxDBPrivate* private = self->private_data;
    if (private) {
        if (private->initialized) {
            switch (private->config.type) {
                case POLYX_DB_TYPE_SQLITE:
                    if (private->db.sqlite) {
                        PeerxSqliteClass.free(private->db.sqlite);
                    }
                    break;
                case POLYX_DB_TYPE_DUCKDB:
                    // TODO: Add DuckDB support
                    break;
            }
        }
        if (private->error_message) {
            g_memory->dealloc(g_memory, private->error_message);
        }
        g_memory->dealloc(g_memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Database operations
static InfraxError polyx_db_open(PolyxDB* self, const polyx_db_config_t* config) {
    if (!self || !config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Close existing connection
    if (private->initialized) {
        polyx_db_close(self);
    }

    // Save configuration
    memcpy(&private->config, config, sizeof(polyx_db_config_t));

    // Open database based on type
    InfraxError err = make_error(INFRAX_ERROR_OK, NULL);
    switch (config->type) {
        case POLYX_DB_TYPE_SQLITE: {
            private->db.sqlite = PeerxSqliteClass.new();
            if (!private->db.sqlite) {
                return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to create SQLite instance");
            }

            peerx_sqlite_conn_info_t info = {
                .read_only = config->read_only,
                .in_memory = (config->url == NULL || strcmp(config->url, ":memory:") == 0),
                .timeout_ms = 5000
            };
            if (!info.in_memory && config->url) {
                strncpy(info.path, config->url, sizeof(info.path) - 1);
            }

            err = private->db.sqlite->open(private->db.sqlite, &info);
            break;
        }
        case POLYX_DB_TYPE_DUCKDB:
            // TODO: Add DuckDB support
            err = make_error(INFRAX_ERROR_NOT_IMPLEMENTED, "DuckDB not supported yet");
            break;
    }

    if (err.code == INFRAX_ERROR_OK) {
        private->initialized = true;
    } else {
        set_error(self, "Failed to open database: %s", err.message);
    }

    return err;
}

static InfraxError polyx_db_close(PolyxDB* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    if (private->initialized) {
        switch (private->config.type) {
            case POLYX_DB_TYPE_SQLITE:
                if (private->db.sqlite) {
                    InfraxError err = private->db.sqlite->close(private->db.sqlite);
                    if (err != INFRAX_OK) {
                        set_error(self, "Failed to close database: %s", err.message);
                        return err;
                    }
                }
                break;
            case POLYX_DB_TYPE_DUCKDB:
                // TODO: Add DuckDB support
                break;
        }
        private->initialized = false;
    }

    return make_error(INFRAX_OK, NULL);
}

static InfraxError polyx_db_exec(PolyxDB* self, const char* sql) {
    if (!self || !sql) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    InfraxError err = make_error(INFRAX_ERROR_OK, NULL);
    switch (private->config.type) {
        case POLYX_DB_TYPE_SQLITE:
            err = private->db.sqlite->exec(private->db.sqlite, sql);
            break;
        case POLYX_DB_TYPE_DUCKDB:
            // TODO: Add DuckDB support
            err = make_error(INFRAX_ERROR_NOT_IMPLEMENTED, "DuckDB not supported yet");
            break;
    }

    if (err.code != INFRAX_ERROR_OK) {
        set_error(self, "Failed to execute SQL: %s", err.message);
    }
    return err;
}

static InfraxError polyx_db_query(PolyxDB* self, const char* sql, polyx_db_result_t* result) {
    if (!self || !sql || !result) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    poly_db_result_t* db_result = NULL;
    InfraxError err = make_error(INFRAX_ERROR_OK, NULL);
    switch (private->config.type) {
        case POLYX_DB_TYPE_SQLITE:
            err = private->db.sqlite->query(private->db.sqlite, sql, &db_result);
            break;
        case POLYX_DB_TYPE_DUCKDB:
            // TODO: Add DuckDB support
            err = make_error(INFRAX_ERROR_NOT_IMPLEMENTED, "DuckDB not supported yet");
            break;
    }

    if (err.code != INFRAX_ERROR_OK) {
        set_error(self, "Failed to execute query: %s", err.message);
        return err;
    }

    // Convert result
    size_t count = 0;
    err = poly_db_result_row_count(db_result, &count);
    if (err != INFRAX_OK) {
        poly_db_result_free(db_result);
        set_error(self, "Failed to get row count: %s", err.message);
        return err;
    }

    result->row_count = count;
    // TODO: Convert column names and row data

    poly_db_result_free(db_result);
    return make_error(INFRAX_OK, NULL);
}

static void polyx_db_free_result(PolyxDB* self, polyx_db_result_t* result) {
    if (!self || !result) return;
    free_result_internal(result);
}

// Transaction management
static InfraxError polyx_db_begin(PolyxDB* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    InfraxError err = polyx_db_exec(self, "BEGIN TRANSACTION");
    if (err != INFRAX_OK) {
        set_error(self, "Failed to begin transaction: %s", err.message);
    }
    return err;
}

static InfraxError polyx_db_commit(PolyxDB* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    InfraxError err = polyx_db_exec(self, "COMMIT");
    if (err != INFRAX_OK) {
        set_error(self, "Failed to commit transaction: %s", err.message);
    }
    return err;
}

static InfraxError polyx_db_rollback(PolyxDB* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    InfraxError err = polyx_db_exec(self, "ROLLBACK");
    if (err != INFRAX_OK) {
        set_error(self, "Failed to rollback transaction: %s", err.message);
    }
    return err;
}

// Key-value operations
static InfraxError polyx_db_set(PolyxDB* self, const char* key, const void* value, size_t value_size) {
    if (!self || !key || (!value && value_size > 0)) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    // Prepare statement
    poly_db_stmt_t* stmt = NULL;
    InfraxError err = poly_db_prepare(private->db.sqlite, 
        "INSERT OR REPLACE INTO kv_store (key, value) VALUES (?, ?)", &stmt);
    if (err != INFRAX_OK) {
        set_error(self, "Failed to prepare statement: %s", err.message);
        return err;
    }

    // Bind parameters
    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err == INFRAX_OK) {
        err = poly_db_bind_blob(stmt, 2, value, value_size);
    }

    // Execute
    if (err == INFRAX_OK) {
        err = poly_db_stmt_step(stmt);
    }

    poly_db_stmt_finalize(stmt);

    if (err != INFRAX_OK) {
        set_error(self, "Failed to set value: %s", err.message);
    }
    return err;
}

static InfraxError polyx_db_get(PolyxDB* self, const char* key, void** value, size_t* value_size) {
    if (!self || !key || !value || !value_size) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    // Prepare statement
    poly_db_stmt_t* stmt = NULL;
    InfraxError err = poly_db_prepare(private->db.sqlite, 
        "SELECT value FROM kv_store WHERE key = ?", &stmt);
    if (err != INFRAX_OK) {
        set_error(self, "Failed to prepare statement: %s", err.message);
        return err;
    }

    // Bind key
    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err != INFRAX_OK) {
        poly_db_stmt_finalize(stmt);
        set_error(self, "Failed to bind key: %s", err.message);
        return err;
    }

    // Execute
    err = poly_db_stmt_step(stmt);
    if (err != INFRAX_OK) {
        poly_db_stmt_finalize(stmt);
        set_error(self, "Failed to execute statement: %s", err.message);
        return err;
    }

    // Get value
    err = poly_db_column_blob(stmt, 0, value, value_size);
    poly_db_stmt_finalize(stmt);

    if (err != INFRAX_OK) {
        set_error(self, "Failed to get value: %s", err.message);
    }
    return err;
}

static InfraxError polyx_db_del(PolyxDB* self, const char* key) {
    if (!self || !key) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Database not open");
    }

    // Prepare statement
    poly_db_stmt_t* stmt = NULL;
    InfraxError err = poly_db_prepare(private->db.sqlite, 
        "DELETE FROM kv_store WHERE key = ?", &stmt);
    if (err != INFRAX_OK) {
        set_error(self, "Failed to prepare statement: %s", err.message);
        return err;
    }

    // Bind key
    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err == INFRAX_OK) {
        err = poly_db_stmt_step(stmt);
    }

    poly_db_stmt_finalize(stmt);

    if (err != INFRAX_OK) {
        set_error(self, "Failed to delete key: %s", err.message);
    }
    return err;
}

static bool polyx_db_exists(PolyxDB* self, const char* key) {
    if (!self || !key) {
        return false;
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return false;
    }

    // Prepare statement
    poly_db_stmt_t* stmt = NULL;
    InfraxError err = poly_db_prepare(private->db.sqlite, 
        "SELECT 1 FROM kv_store WHERE key = ?", &stmt);
    if (err != INFRAX_OK) {
        set_error(self, "Failed to prepare statement: %s", err.message);
        return false;
    }

    // Bind key
    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err == INFRAX_OK) {
        err = poly_db_stmt_step(stmt);
    }

    bool exists = (err == INFRAX_OK);
    poly_db_stmt_finalize(stmt);

    return exists;
}

// Status and error handling
static InfraxError polyx_db_get_status(PolyxDB* self, char* status, size_t size) {
    if (!self || !status || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    const char* type_str = private->initialized ? 
        (private->config.type == POLYX_DB_TYPE_SQLITE ? "SQLite" : "DuckDB") : 
        "Not connected";
    const char* tx_str = private->initialized ? "In transaction" : "No transaction";

    snprintf(status, size, "Type: %s, State: %s", type_str, tx_str);
    return make_error(INFRAX_OK, NULL);
}

static const char* polyx_db_get_error(PolyxDB* self) {
    if (!self) return NULL;
    PolyxDBPrivate* private = self->private_data;
    return private ? private->error_message : NULL;
}

static void polyx_db_clear_error(PolyxDB* self) {
    if (!self) return;
    PolyxDBPrivate* private = self->private_data;
    if (private && private->error_message) {
        g_memory->dealloc(g_memory, private->error_message);
        private->error_message = NULL;
    }
}

// Helper functions
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

static void free_result_internal(polyx_db_result_t* result) {
    if (!result || !g_memory) return;

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

    memset(result, 0, sizeof(polyx_db_result_t));
}

static void set_error(PolyxDB* self, const char* format, ...) {
    if (!self) return;
    PolyxDBPrivate* private = self->private_data;
    if (!private) return;

    // Clear old error
    if (private->error_message) {
        g_memory->dealloc(g_memory, private->error_message);
        private->error_message = NULL;
    }

    // Format new error
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Save new error
    private->error_message = g_memory->alloc(g_memory, strlen(buffer) + 1);
    if (private->error_message) {
        strcpy(private->error_message, buffer);
    }
}

// Global class instance
const PolyxDBClassType PolyxDBClass = {
    .new = polyx_db_new,
    .free = polyx_db_free,
    .open = polyx_db_open,
    .close = polyx_db_close,
    .exec = polyx_db_exec,
    .query = polyx_db_query,
    .free_result = polyx_db_free_result,
    .begin = polyx_db_begin,
    .commit = polyx_db_commit,
    .rollback = polyx_db_rollback,
    .set = polyx_db_set,
    .get = polyx_db_get,
    .del = polyx_db_del,
    .exists = polyx_db_exists,
    .get_status = polyx_db_get_status,
    .get_error = polyx_db_get_error,
    .clear_error = polyx_db_clear_error
}; 