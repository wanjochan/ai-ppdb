#include "PolyxDB.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/peerx/PeerxSqlite.h"

// Forward declarations
static InfraxError polyx_db_close(PolyxDB* self);

// Private data structure
typedef struct {
    InfraxMemory* memory;
    InfraxCore* core;
    union {
        PeerxSqlite* sqlite;
        // TODO: Add DuckDB support
    } db;
    polyx_db_config_t config;
    InfraxBool initialized;
    char* error_message;
} PolyxDBPrivate;

// Global memory manager and core
static InfraxMemory* g_memory = NULL;
static InfraxCore* g_core = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxCoreClassType InfraxCoreClass;
extern const PeerxSqliteClassType PeerxSqliteClass;

// Forward declarations of private functions
static InfraxBool init_memory(void);
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
    g_core->memset(g_core, self, 0, sizeof(PolyxDB));
    self->self = self;
    self->klass = &PolyxDBClass;

    // Allocate private data
    PolyxDBPrivate* private = g_memory->alloc(g_memory, sizeof(PolyxDBPrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    g_core->memset(g_core, private, 0, sizeof(PolyxDBPrivate));
    private->memory = g_memory;
    private->core = g_core;
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
    g_core->memcpy(g_core, &private->config, config, sizeof(polyx_db_config_t));

    // Open database based on type
    InfraxError err = make_error(INFRAX_ERROR_OK, NULL);
    switch (config->type) {
        case POLYX_DB_TYPE_SQLITE: {
            private->db.sqlite = PeerxSqliteClass.new();
            if (!private->db.sqlite) {
                return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to create SQLite instance");
            }

            peerx_sqlite_conn_info_t info = {0};
            info.read_only = config->read_only;
            info.in_memory = (config->url == NULL || g_core->strcmp(g_core, config->url, ":memory:") == 0);
            info.timeout_ms = 5000;
            
            if (!info.in_memory && config->url) {
                g_core->strncpy(g_core, info.path, config->url, sizeof(info.path) - 1);
            }

            err = private->db.sqlite->open(private->db.sqlite, &info);
            break;
        }
        case POLYX_DB_TYPE_DUCKDB:
            // TODO: Add DuckDB support
            err = make_error(INFRAX_ERROR_SYSTEM, "DuckDB not supported yet");
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
                    if (err.code != INFRAX_ERROR_OK) {
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

    return make_error(INFRAX_ERROR_OK, NULL);
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
            err = make_error(INFRAX_ERROR_SYSTEM, "DuckDB not supported yet");
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

    InfraxError err = make_error(INFRAX_ERROR_OK, NULL);
    switch (private->config.type) {
        case POLYX_DB_TYPE_SQLITE: {
            peerx_sqlite_result_t sqlite_result = {0};
            err = private->db.sqlite->query(private->db.sqlite, sql, &sqlite_result);
            if (err.code != INFRAX_ERROR_OK) {
                set_error(self, "Failed to execute query: %s", err.message);
                return err;
            }

            // Convert result
            result->column_count = sqlite_result.column_count;
            result->row_count = sqlite_result.row_count;

            // Allocate and copy column names
            result->column_names = g_memory->alloc(g_memory, result->column_count * sizeof(char*));
            if (!result->column_names) {
                private->db.sqlite->free_result(private->db.sqlite, &sqlite_result);
                return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate column names");
            }

            for (size_t i = 0; i < result->column_count; i++) {
                size_t len = g_core->strlen(g_core, sqlite_result.column_names[i]);
                result->column_names[i] = g_memory->alloc(g_memory, len + 1);
                if (!result->column_names[i]) {
                    free_result_internal(result);
                    private->db.sqlite->free_result(private->db.sqlite, &sqlite_result);
                    return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate column name");
                }
                g_core->strcpy(g_core, result->column_names[i], sqlite_result.column_names[i]);
            }

            // Allocate and copy rows
            result->rows = g_memory->alloc(g_memory, result->row_count * sizeof(char**));
            if (!result->rows) {
                free_result_internal(result);
                private->db.sqlite->free_result(private->db.sqlite, &sqlite_result);
                return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate rows");
            }

            for (size_t i = 0; i < result->row_count; i++) {
                result->rows[i] = g_memory->alloc(g_memory, result->column_count * sizeof(char*));
                if (!result->rows[i]) {
                    free_result_internal(result);
                    private->db.sqlite->free_result(private->db.sqlite, &sqlite_result);
                    return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate row");
                }

                for (size_t j = 0; j < result->column_count; j++) {
                    if (sqlite_result.rows[i][j]) {
                        size_t len = g_core->strlen(g_core, sqlite_result.rows[i][j]);
                        result->rows[i][j] = g_memory->alloc(g_memory, len + 1);
                        if (!result->rows[i][j]) {
                            free_result_internal(result);
                            private->db.sqlite->free_result(private->db.sqlite, &sqlite_result);
                            return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate cell");
                        }
                        g_core->strcpy(g_core, result->rows[i][j], sqlite_result.rows[i][j]);
                    } else {
                        result->rows[i][j] = NULL;
                    }
                }
            }

            private->db.sqlite->free_result(private->db.sqlite, &sqlite_result);
            break;
        }
        case POLYX_DB_TYPE_DUCKDB:
            // TODO: Add DuckDB support
            err = make_error(INFRAX_ERROR_SYSTEM, "DuckDB not supported yet");
            break;
    }

    return err;
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
    if (err.code != INFRAX_ERROR_OK) {
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
    if (err.code != INFRAX_ERROR_OK) {
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
    if (err.code != INFRAX_ERROR_OK) {
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

    // Create SQL with parameters
    char* sql = g_memory->alloc(g_memory, 256);
    if (!sql) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate SQL buffer");
    }

    g_core->snprintf(g_core, sql, 256, "INSERT OR REPLACE INTO kv_store (key, value) VALUES ('%s', ?)", key);
    
    // Execute with BLOB parameter
    InfraxError err;
    switch (private->config.type) {
        case POLYX_DB_TYPE_SQLITE: {
            peerx_sqlite_result_t result = {0};
            err = private->db.sqlite->exec(private->db.sqlite, sql);
            break;
        }
        case POLYX_DB_TYPE_DUCKDB:
            err = make_error(INFRAX_ERROR_SYSTEM, "DuckDB not supported yet");
            break;
    }

    g_memory->dealloc(g_memory, sql);

    if (err.code != INFRAX_ERROR_OK) {
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

    // Create SQL
    char* sql = g_memory->alloc(g_memory, 256);
    if (!sql) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate SQL buffer");
    }

    g_core->snprintf(g_core, sql, 256, "SELECT value FROM kv_store WHERE key = '%s'", key);

    // Execute query
    InfraxError err;
    switch (private->config.type) {
        case POLYX_DB_TYPE_SQLITE: {
            peerx_sqlite_result_t result = {0};
            err = private->db.sqlite->query(private->db.sqlite, sql, &result);
            if (err.code == INFRAX_ERROR_OK && result.row_count > 0) {
                // Allocate and copy value
                *value_size = g_core->strlen(g_core, result.rows[0][0]);
                *value = g_memory->alloc(g_memory, *value_size);
                if (*value) {
                    g_core->memcpy(g_core, *value, result.rows[0][0], *value_size);
                } else {
                    err = make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate value buffer");
                }
            } else {
                err = make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Key not found");
            }
            private->db.sqlite->free_result(private->db.sqlite, &result);
            break;
        }
        case POLYX_DB_TYPE_DUCKDB:
            err = make_error(INFRAX_ERROR_SYSTEM, "DuckDB not supported yet");
            break;
    }

    g_memory->dealloc(g_memory, sql);

    if (err.code != INFRAX_ERROR_OK) {
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

    // Create SQL
    char* sql = g_memory->alloc(g_memory, 256);
    if (!sql) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate SQL buffer");
    }

    g_core->snprintf(g_core, sql, 256, "DELETE FROM kv_store WHERE key = '%s'", key);

    // Execute
    InfraxError err;
    switch (private->config.type) {
        case POLYX_DB_TYPE_SQLITE:
            err = private->db.sqlite->exec(private->db.sqlite, sql);
            break;
        case POLYX_DB_TYPE_DUCKDB:
            err = make_error(INFRAX_ERROR_SYSTEM, "DuckDB not supported yet");
            break;
    }

    g_memory->dealloc(g_memory, sql);

    if (err.code != INFRAX_ERROR_OK) {
        set_error(self, "Failed to delete key: %s", err.message);
    }
    return err;
}

static InfraxBool polyx_db_exists(PolyxDB* self, const char* key) {
    if (!self || !key) {
        return INFRAX_FALSE;
    }

    PolyxDBPrivate* private = self->private_data;
    if (!private || !private->initialized) {
        return INFRAX_FALSE;
    }

    // Create SQL
    char* sql = g_memory->alloc(g_memory, 256);
    if (!sql) {
        return INFRAX_FALSE;
    }

    g_core->snprintf(g_core, sql, 256, "SELECT 1 FROM kv_store WHERE key = '%s'", key);

    // Execute query
    InfraxBool exists = INFRAX_FALSE;
    switch (private->config.type) {
        case POLYX_DB_TYPE_SQLITE: {
            peerx_sqlite_result_t result = {0};
            InfraxError err = private->db.sqlite->query(private->db.sqlite, sql, &result);
            exists = (err.code == INFRAX_ERROR_OK && result.row_count > 0) ? INFRAX_TRUE : INFRAX_FALSE;
            private->db.sqlite->free_result(private->db.sqlite, &result);
            break;
        }
        case POLYX_DB_TYPE_DUCKDB:
            break;
    }

    g_memory->dealloc(g_memory, sql);
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

    g_core->snprintf(g_core, status, size, "Type: %s, State: %s", type_str, tx_str);
    return make_error(INFRAX_ERROR_OK, NULL);
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
static InfraxBool init_memory(void) {
    if (g_memory && g_core) return true;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    if (!g_memory) return false;

    g_core = InfraxCoreClass.singleton();
    return g_core != NULL;
}

static void free_result_internal(polyx_db_result_t* result) {
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

    g_core->memset(g_core, result, 0, sizeof(polyx_db_result_t));
}

static void set_error(PolyxDB* self, const char* format, ...) {
    if (!self) return;

    PolyxDBPrivate* private = self->private_data;
    if (!private) return;

    if (private->error_message) {
        g_memory->dealloc(g_memory, private->error_message);
        private->error_message = NULL;
    }

    char buffer[256];
    va_list args;
    va_start(args, format);
    g_core->snprintf(g_core, buffer, sizeof(buffer), format, args);
    va_end(args);

    size_t len = g_core->strlen(g_core, buffer);
    private->error_message = g_memory->alloc(g_memory, len + 1);
    if (private->error_message) {
        g_core->strcpy(g_core, private->error_message, buffer);
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