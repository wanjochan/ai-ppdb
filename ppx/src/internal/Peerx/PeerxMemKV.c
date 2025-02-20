#include "PeerxMemKV.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/polyx/PolyxDB.h"

// #include <string.h>
// #include <time.h>

#define MAX_BUCKETS 1024
#define LOAD_FACTOR_THRESHOLD 0.75

// Hash table entry
typedef struct peerx_memkv_entry {
    peerx_memkv_pair_t pair;
    struct peerx_memkv_entry* next;
} peerx_memkv_entry_t;

// Private data structure
typedef struct {
    InfraxMemory* memory;
    PolyxDB* db;
    bool initialized;
} PeerxMemKVPrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern PolyxDBClassType PolyxDBClass;

// Forward declarations of private functions
static bool init_memory(void);
static void cleanup_entry(PeerxMemKVPrivate* private, peerx_memkv_entry_t* entry);
static bool is_expired(const peerx_memkv_entry_t* entry);
static void cleanup_expired(PeerxMemKV* self);
static void free_value_internal(peerx_memkv_value_t* value);
static InfraxError serialize_value(const peerx_memkv_value_t* value, void** data, size_t* size);
static InfraxError deserialize_value(const void* data, size_t size, peerx_memkv_value_t* value);

// Constructor
static PeerxMemKV* peerx_memkv_new(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Allocate instance
    PeerxMemKV* self = g_memory->alloc(g_memory, sizeof(PeerxMemKV));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    memset(self, 0, sizeof(PeerxMemKV));
    
    // Initialize base service
    PeerxService* base = PeerxServiceClass.new();
    if (!base) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    memcpy(&self->base, base, sizeof(PeerxService));
    g_memory->dealloc(g_memory, base);

    // Allocate private data
    PeerxMemKVPrivate* private = g_memory->alloc(g_memory, sizeof(PeerxMemKVPrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    memset(private, 0, sizeof(PeerxMemKVPrivate));
    private->memory = g_memory;

    // Create database instance
    private->db = PolyxDBClass.new();
    if (!private->db) {
        g_memory->dealloc(g_memory, private);
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    self->base.private_data = private;

    // Initialize function pointers
    self->set = peerx_memkv_set;
    self->set_ex = peerx_memkv_set_ex;
    self->get = peerx_memkv_get;
    self->del = peerx_memkv_del;
    self->exists = peerx_memkv_exists;
    self->multi_set = peerx_memkv_multi_set;
    self->multi_get = peerx_memkv_multi_get;
    self->multi_del = peerx_memkv_multi_del;
    self->keys = peerx_memkv_keys;
    self->expire = peerx_memkv_expire;
    self->ttl = peerx_memkv_ttl;
    self->flush = peerx_memkv_flush;
    self->info = peerx_memkv_info;

    return self;
}

// Destructor
static void peerx_memkv_free(PeerxMemKV* self) {
    if (!self) return;

    // Stop service if running
    if (self->base.is_running) {
        self->base.klass->stop(&self->base);
    }

    // Free private data
    PeerxMemKVPrivate* private = self->base.private_data;
    if (private) {
        if (private->db) {
            PolyxDBClass.free(private->db);
        }
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Key-value operations
static InfraxError peerx_memkv_set(PeerxMemKV* self, const char* key, const peerx_memkv_value_t* value) {
    if (!self || !key || !value) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Serialize value
    void* data = NULL;
    size_t size = 0;
    InfraxError err = serialize_value(value, &data, &size);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Store value
    err = private->db->set(private->db, key, data, size);
    g_memory->dealloc(g_memory, data);

    return err;
}

static InfraxError peerx_memkv_set_ex(PeerxMemKV* self, const char* key, 
                                     const peerx_memkv_value_t* value, int64_t ttl_ms) {
    if (!self || !key || !value) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Begin transaction
    InfraxError err = private->db->begin(private->db);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Set value
    err = peerx_memkv_set(self, key, value);
    if (err.code != INFRAX_ERROR_OK) {
        private->db->rollback(private->db);
        return err;
    }

    // Set expiration
    if (ttl_ms > 0) {
        char sql[256];
        snprintf(sql, sizeof(sql), 
            "UPDATE kv_store SET expire_at = ? WHERE key = ?");
        
        int64_t expire_at = time(NULL) * 1000 + ttl_ms;
        
        // TODO: Execute update
    }

    // Commit transaction
    return private->db->commit(private->db);
}

static InfraxError peerx_memkv_get(PeerxMemKV* self, const char* key, peerx_memkv_value_t* value) {
    if (!self || !key || !value) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Get value
    void* data = NULL;
    size_t size = 0;
    InfraxError err = private->db->get(private->db, key, &data, &size);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Deserialize value
    err = deserialize_value(data, size, value);
    g_memory->dealloc(g_memory, data);

    return err;
}

static InfraxError peerx_memkv_del(PeerxMemKV* self, const char* key) {
    if (!self || !key) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    return private->db->del(private->db, key);
}

static bool peerx_memkv_exists(PeerxMemKV* self, const char* key) {
    if (!self || !key) {
        return false;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return false;
    }

    return private->db->exists(private->db, key);
}

// Batch operations
static InfraxError peerx_memkv_multi_set(PeerxMemKV* self, const peerx_memkv_pair_t* pairs, size_t count) {
    if (!self || !pairs || count == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Begin transaction
    InfraxError err = private->db->begin(private->db);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Set all values
    for (size_t i = 0; i < count; i++) {
        err = peerx_memkv_set(self, pairs[i].key, &pairs[i].value);
        if (err.code != INFRAX_ERROR_OK) {
            private->db->rollback(private->db);
            return err;
        }
    }

    // Commit transaction
    return private->db->commit(private->db);
}

static InfraxError peerx_memkv_multi_get(PeerxMemKV* self, const char** keys, size_t key_count,
                                        peerx_memkv_pair_t* pairs, size_t* pair_count) {
    if (!self || !keys || !pairs || !pair_count || key_count == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    size_t count = 0;
    InfraxError err = make_error(INFRAX_ERROR_OK, NULL);

    // Get all values
    for (size_t i = 0; i < key_count && count < *pair_count; i++) {
        strncpy(pairs[count].key, keys[i], sizeof(pairs[count].key) - 1);
        err = peerx_memkv_get(self, keys[i], &pairs[count].value);
        if (err.code == INFRAX_ERROR_OK) {
            count++;
        }
    }

    *pair_count = count;
    return err;
}

static InfraxError peerx_memkv_multi_del(PeerxMemKV* self, const char** keys, size_t count) {
    if (!self || !keys || count == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Begin transaction
    InfraxError err = private->db->begin(private->db);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Delete all keys
    for (size_t i = 0; i < count; i++) {
        err = peerx_memkv_del(self, keys[i]);
        if (err.code != INFRAX_ERROR_OK) {
            private->db->rollback(private->db);
            return err;
        }
    }

    // Commit transaction
    return private->db->commit(private->db);
}

// Key operations
static InfraxError peerx_memkv_keys(PeerxMemKV* self, const char* pattern, char** keys, size_t* count) {
    if (!self || !pattern || !keys || !count) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // TODO: Implement pattern matching
    return make_error(INFRAX_ERROR_NOT_IMPLEMENTED, "Pattern matching not implemented");
}

static InfraxError peerx_memkv_expire(PeerxMemKV* self, const char* key, int64_t ttl_ms) {
    if (!self || !key) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // TODO: Implement expiration
    return make_error(INFRAX_ERROR_NOT_IMPLEMENTED, "Expiration not implemented");
}

static InfraxError peerx_memkv_ttl(PeerxMemKV* self, const char* key, int64_t* ttl_ms) {
    if (!self || !key || !ttl_ms) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // TODO: Implement TTL query
    return make_error(INFRAX_ERROR_NOT_IMPLEMENTED, "TTL query not implemented");
}

// Server operations
static InfraxError peerx_memkv_flush(PeerxMemKV* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    return private->db->exec(private->db, "DELETE FROM kv_store");
}

static InfraxError peerx_memkv_info(PeerxMemKV* self, char* info, size_t size) {
    if (!self || !info || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    return private->db->get_status(private->db, info, size);
}

// Service lifecycle
static InfraxError peerx_memkv_init(PeerxMemKV* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Initialize base service
    InfraxError err = self->base.klass->init(&self->base, config);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Configure database
    polyx_db_config_t db_config = {
        .type = POLY_DB_TYPE_SQLITE,
        .url = ":memory:",
        .max_memory = 0,
        .read_only = false,
        .plugin_path = NULL,
        .allow_fallback = false
    };

    // Open database
    err = private->db->open(private->db, &db_config);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Create table
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "  key TEXT PRIMARY KEY,"
        "  value BLOB,"
        "  type INTEGER,"
        "  expire_at INTEGER"
        ")";
    
    err = private->db->exec(private->db, sql);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    private->initialized = true;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_memkv_start(PeerxMemKV* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private || !private->db || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Start base service
    return self->base.klass->start(&self->base);
}

static InfraxError peerx_memkv_stop(PeerxMemKV* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    // Stop base service
    return self->base.klass->stop(&self->base);
}

static InfraxError peerx_memkv_reload(PeerxMemKV* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    // Reload base service
    return self->base.klass->reload(&self->base);
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

static void cleanup_entry(PeerxMemKVPrivate* private, peerx_memkv_entry_t* entry) {
    if (!private || !entry) return;
    peerx_memkv_value_free(&entry->pair.value);
    private->memory->dealloc(private->memory, entry);
}

static bool is_expired(const peerx_memkv_entry_t* entry) {
    if (!entry || entry->pair.expire_at == 0) {
        return false;
    }
    return (time(NULL) * 1000) >= entry->pair.expire_at;
}

static void cleanup_expired(PeerxMemKV* self) {
    if (!self) return;

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) return;

    for (int i = 0; i < MAX_BUCKETS; i++) {
        peerx_memkv_entry_t** pp = &private->buckets[i];
        peerx_memkv_entry_t* entry = *pp;
        while (entry) {
            if (is_expired(entry)) {
                *pp = entry->next;
                cleanup_entry(private, entry);
                private->size--;
                entry = *pp;
            } else {
                pp = &entry->next;
                entry = entry->next;
            }
        }
    }
}

static void free_value_internal(peerx_memkv_value_t* value) {
    if (!value) return;

    switch (value->type) {
        case PEERX_MEMKV_TYPE_STRING:
            if (value->value.str) {
                g_memory->dealloc(g_memory, value->value.str);
            }
            break;
        case PEERX_MEMKV_TYPE_BINARY:
            if (value->value.bin.data) {
                g_memory->dealloc(g_memory, value->value.bin.data);
            }
            break;
        default:
            break;
    }

    memset(value, 0, sizeof(peerx_memkv_value_t));
}

static InfraxError serialize_value(const peerx_memkv_value_t* value, void** data, size_t* size) {
    if (!value || !data || !size) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Calculate size
    size_t total_size = sizeof(peerx_memkv_type_t);
    switch (value->type) {
        case PEERX_MEMKV_TYPE_STRING:
            if (value->value.str) {
                total_size += strlen(value->value.str) + 1;
            }
            break;
        case PEERX_MEMKV_TYPE_INT:
            total_size += sizeof(int64_t);
            break;
        case PEERX_MEMKV_TYPE_FLOAT:
            total_size += sizeof(double);
            break;
        case PEERX_MEMKV_TYPE_BINARY:
            if (value->value.bin.data) {
                total_size += sizeof(size_t) + value->value.bin.size;
            }
            break;
    }

    // Allocate buffer
    void* buffer = g_memory->alloc(g_memory, total_size);
    if (!buffer) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate buffer");
    }

    // Serialize data
    char* ptr = buffer;
    memcpy(ptr, &value->type, sizeof(peerx_memkv_type_t));
    ptr += sizeof(peerx_memkv_type_t);

    switch (value->type) {
        case PEERX_MEMKV_TYPE_STRING:
            if (value->value.str) {
                strcpy(ptr, value->value.str);
            }
            break;
        case PEERX_MEMKV_TYPE_INT:
            memcpy(ptr, &value->value.i, sizeof(int64_t));
            break;
        case PEERX_MEMKV_TYPE_FLOAT:
            memcpy(ptr, &value->value.f, sizeof(double));
            break;
        case PEERX_MEMKV_TYPE_BINARY:
            if (value->value.bin.data) {
                memcpy(ptr, &value->value.bin.size, sizeof(size_t));
                ptr += sizeof(size_t);
                memcpy(ptr, value->value.bin.data, value->value.bin.size);
            }
            break;
    }

    *data = buffer;
    *size = total_size;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError deserialize_value(const void* data, size_t size, peerx_memkv_value_t* value) {
    if (!data || !value || size < sizeof(peerx_memkv_type_t)) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    const char* ptr = data;
    memcpy(&value->type, ptr, sizeof(peerx_memkv_type_t));
    ptr += sizeof(peerx_memkv_type_t);

    switch (value->type) {
        case PEERX_MEMKV_TYPE_STRING: {
            size_t str_len = strlen(ptr);
            value->value.str = g_memory->alloc(g_memory, str_len + 1);
            if (!value->value.str) {
                return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate string");
            }
            strcpy(value->value.str, ptr);
            break;
        }
        case PEERX_MEMKV_TYPE_INT:
            memcpy(&value->value.i, ptr, sizeof(int64_t));
            break;
        case PEERX_MEMKV_TYPE_FLOAT:
            memcpy(&value->value.f, ptr, sizeof(double));
            break;
        case PEERX_MEMKV_TYPE_BINARY: {
            memcpy(&value->value.bin.size, ptr, sizeof(size_t));
            ptr += sizeof(size_t);
            value->value.bin.data = g_memory->alloc(g_memory, value->value.bin.size);
            if (!value->value.bin.data) {
                return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate binary data");
            }
            memcpy(value->value.bin.data, ptr, value->value.bin.size);
            break;
        }
        default:
            return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid value type");
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Global class instance
const PeerxMemKVClassType PeerxMemKVClass = {
    .new = peerx_memkv_new,
    .free = peerx_memkv_free,
    .init = (InfraxError (*)(PeerxMemKV*, const polyx_service_config_t*))peerx_memkv_init,
    .start = (InfraxError (*)(PeerxMemKV*))peerx_memkv_start,
    .stop = (InfraxError (*)(PeerxMemKV*))peerx_memkv_stop,
    .reload = (InfraxError (*)(PeerxMemKV*))peerx_memkv_reload,
    .get_status = (InfraxError (*)(PeerxMemKV*, char*, size_t))PeerxServiceClass.get_status,
    .get_error = (const char* (*)(PeerxMemKV*))PeerxServiceClass.get_error,
    .clear_error = (void (*)(PeerxMemKV*))PeerxServiceClass.clear_error,
    .validate_config = (InfraxError (*)(PeerxMemKV*, const polyx_service_config_t*))PeerxServiceClass.validate_config,
    .apply_config = (InfraxError (*)(PeerxMemKV*, const polyx_service_config_t*))PeerxServiceClass.apply_config
}; 