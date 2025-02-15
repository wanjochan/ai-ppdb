#include "PeerxMemKV.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxHash.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

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
    peerx_memkv_entry_t* buckets[MAX_BUCKETS];
    size_t size;
    InfraxHash* hash;
} PeerxMemKVPrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxHashClassType InfraxHashClass;

// Forward declarations of private functions
static bool init_memory(void);
static void cleanup_entry(PeerxMemKVPrivate* private, peerx_memkv_entry_t* entry);
static bool is_expired(const peerx_memkv_entry_t* entry);
static void cleanup_expired(PeerxMemKV* self);

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
    private->hash = InfraxHashClass.new();
    if (!private->hash) {
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
        // Free all entries
        for (int i = 0; i < MAX_BUCKETS; i++) {
            peerx_memkv_entry_t* entry = private->buckets[i];
            while (entry) {
                peerx_memkv_entry_t* next = entry->next;
                cleanup_entry(private, entry);
                entry = next;
            }
        }

        if (private->hash) {
            InfraxHashClass.free(private->hash);
        }
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Key-value operations
static infrax_error_t peerx_memkv_set(PeerxMemKV* self, const char* key, 
                                     const peerx_memkv_value_t* value) {
    return peerx_memkv_set_ex(self, key, value, 0);
}

static infrax_error_t peerx_memkv_set_ex(PeerxMemKV* self, const char* key, 
                                        const peerx_memkv_value_t* value,
                                        int64_t ttl_ms) {
    if (!self || !key || !value) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Calculate hash
    uint32_t hash = InfraxHashClass.hash(private->hash, key, strlen(key));
    uint32_t bucket = hash % MAX_BUCKETS;

    // Check if key exists
    peerx_memkv_entry_t* entry = private->buckets[bucket];
    while (entry) {
        if (strcmp(entry->pair.key, key) == 0) {
            // Update value
            peerx_memkv_value_free(&entry->pair.value);
            infrax_error_t err = peerx_memkv_value_copy(&entry->pair.value, value);
            if (err != INFRAX_OK) {
                return err;
            }
            entry->pair.expire_at = ttl_ms ? (time(NULL) * 1000 + ttl_ms) : 0;
            return INFRAX_OK;
        }
        entry = entry->next;
    }

    // Create new entry
    entry = g_memory->alloc(g_memory, sizeof(peerx_memkv_entry_t));
    if (!entry) {
        return INFRAX_ERROR_NO_MEMORY;
    }

    // Initialize entry
    memset(entry, 0, sizeof(peerx_memkv_entry_t));
    strncpy(entry->pair.key, key, sizeof(entry->pair.key) - 1);
    infrax_error_t err = peerx_memkv_value_copy(&entry->pair.value, value);
    if (err != INFRAX_OK) {
        g_memory->dealloc(g_memory, entry);
        return err;
    }
    entry->pair.expire_at = ttl_ms ? (time(NULL) * 1000 + ttl_ms) : 0;

    // Add to bucket
    entry->next = private->buckets[bucket];
    private->buckets[bucket] = entry;
    private->size++;

    return INFRAX_OK;
}

static infrax_error_t peerx_memkv_get(PeerxMemKV* self, const char* key, 
                                     peerx_memkv_value_t* value) {
    if (!self || !key || !value) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Calculate hash
    uint32_t hash = InfraxHashClass.hash(private->hash, key, strlen(key));
    uint32_t bucket = hash % MAX_BUCKETS;

    // Find key
    peerx_memkv_entry_t* entry = private->buckets[bucket];
    while (entry) {
        if (strcmp(entry->pair.key, key) == 0) {
            if (is_expired(entry)) {
                cleanup_expired(self);
                return INFRAX_ERROR_NOT_FOUND;
            }
            return peerx_memkv_value_copy(value, &entry->pair.value);
        }
        entry = entry->next;
    }

    return INFRAX_ERROR_NOT_FOUND;
}

static infrax_error_t peerx_memkv_del(PeerxMemKV* self, const char* key) {
    if (!self || !key) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Calculate hash
    uint32_t hash = InfraxHashClass.hash(private->hash, key, strlen(key));
    uint32_t bucket = hash % MAX_BUCKETS;

    // Find and remove key
    peerx_memkv_entry_t** pp = &private->buckets[bucket];
    peerx_memkv_entry_t* entry = *pp;
    while (entry) {
        if (strcmp(entry->pair.key, key) == 0) {
            *pp = entry->next;
            cleanup_entry(private, entry);
            private->size--;
            return INFRAX_OK;
        }
        pp = &entry->next;
        entry = entry->next;
    }

    return INFRAX_ERROR_NOT_FOUND;
}

static bool peerx_memkv_exists(PeerxMemKV* self, const char* key) {
    if (!self || !key) {
        return false;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return false;
    }

    // Calculate hash
    uint32_t hash = InfraxHashClass.hash(private->hash, key, strlen(key));
    uint32_t bucket = hash % MAX_BUCKETS;

    // Find key
    peerx_memkv_entry_t* entry = private->buckets[bucket];
    while (entry) {
        if (strcmp(entry->pair.key, key) == 0) {
            if (is_expired(entry)) {
                cleanup_expired(self);
                return false;
            }
            return true;
        }
        entry = entry->next;
    }

    return false;
}

// Batch operations
static infrax_error_t peerx_memkv_multi_set(PeerxMemKV* self, 
                                           const peerx_memkv_pair_t* pairs,
                                           size_t count) {
    if (!self || !pairs || count == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < count; i++) {
        infrax_error_t err = peerx_memkv_set_ex(self, pairs[i].key, &pairs[i].value,
                                               pairs[i].expire_at);
        if (err != INFRAX_OK) {
            return err;
        }
    }

    return INFRAX_OK;
}

static infrax_error_t peerx_memkv_multi_get(PeerxMemKV* self, const char** keys,
                                           size_t key_count,
                                           peerx_memkv_pair_t* pairs,
                                           size_t* pair_count) {
    if (!self || !keys || !pairs || !pair_count || key_count == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    size_t found = 0;
    for (size_t i = 0; i < key_count && found < *pair_count; i++) {
        peerx_memkv_value_t value;
        peerx_memkv_value_init(&value);
        
        infrax_error_t err = peerx_memkv_get(self, keys[i], &value);
        if (err == INFRAX_OK) {
            strncpy(pairs[found].key, keys[i], sizeof(pairs[found].key) - 1);
            memcpy(&pairs[found].value, &value, sizeof(peerx_memkv_value_t));
            found++;
        }
    }

    *pair_count = found;
    return INFRAX_OK;
}

static infrax_error_t peerx_memkv_multi_del(PeerxMemKV* self, const char** keys,
                                           size_t count) {
    if (!self || !keys || count == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < count; i++) {
        peerx_memkv_del(self, keys[i]);
    }

    return INFRAX_OK;
}

// Key operations
static infrax_error_t peerx_memkv_keys(PeerxMemKV* self, const char* pattern,
                                      char** keys, size_t* count) {
    if (!self || !keys || !count || *count == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    size_t found = 0;
    for (int i = 0; i < MAX_BUCKETS && found < *count; i++) {
        peerx_memkv_entry_t* entry = private->buckets[i];
        while (entry && found < *count) {
            if (!is_expired(entry) && (!pattern || strstr(entry->pair.key, pattern))) {
                keys[found] = g_memory->alloc(g_memory, strlen(entry->pair.key) + 1);
                if (!keys[found]) {
                    for (size_t j = 0; j < found; j++) {
                        g_memory->dealloc(g_memory, keys[j]);
                    }
                    return INFRAX_ERROR_NO_MEMORY;
                }
                strcpy(keys[found], entry->pair.key);
                found++;
            }
            entry = entry->next;
        }
    }

    *count = found;
    return INFRAX_OK;
}

static infrax_error_t peerx_memkv_expire(PeerxMemKV* self, const char* key,
                                        int64_t ttl_ms) {
    if (!self || !key) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Calculate hash
    uint32_t hash = InfraxHashClass.hash(private->hash, key, strlen(key));
    uint32_t bucket = hash % MAX_BUCKETS;

    // Find key
    peerx_memkv_entry_t* entry = private->buckets[bucket];
    while (entry) {
        if (strcmp(entry->pair.key, key) == 0) {
            if (is_expired(entry)) {
                cleanup_expired(self);
                return INFRAX_ERROR_NOT_FOUND;
            }
            entry->pair.expire_at = ttl_ms ? (time(NULL) * 1000 + ttl_ms) : 0;
            return INFRAX_OK;
        }
        entry = entry->next;
    }

    return INFRAX_ERROR_NOT_FOUND;
}

static infrax_error_t peerx_memkv_ttl(PeerxMemKV* self, const char* key,
                                     int64_t* ttl_ms) {
    if (!self || !key || !ttl_ms) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Calculate hash
    uint32_t hash = InfraxHashClass.hash(private->hash, key, strlen(key));
    uint32_t bucket = hash % MAX_BUCKETS;

    // Find key
    peerx_memkv_entry_t* entry = private->buckets[bucket];
    while (entry) {
        if (strcmp(entry->pair.key, key) == 0) {
            if (is_expired(entry)) {
                cleanup_expired(self);
                return INFRAX_ERROR_NOT_FOUND;
            }
            if (entry->pair.expire_at == 0) {
                *ttl_ms = -1;  // No expiration
            } else {
                int64_t now = time(NULL) * 1000;
                *ttl_ms = entry->pair.expire_at - now;
                if (*ttl_ms < 0) {
                    *ttl_ms = 0;
                }
            }
            return INFRAX_OK;
        }
        entry = entry->next;
    }

    return INFRAX_ERROR_NOT_FOUND;
}

// Server operations
static infrax_error_t peerx_memkv_flush(PeerxMemKV* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Free all entries
    for (int i = 0; i < MAX_BUCKETS; i++) {
        peerx_memkv_entry_t* entry = private->buckets[i];
        while (entry) {
            peerx_memkv_entry_t* next = entry->next;
            cleanup_entry(private, entry);
            entry = next;
        }
        private->buckets[i] = NULL;
    }

    private->size = 0;
    return INFRAX_OK;
}

static infrax_error_t peerx_memkv_info(PeerxMemKV* self, char* info, size_t size) {
    if (!self || !info || size == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Count non-empty buckets
    size_t used_buckets = 0;
    for (int i = 0; i < MAX_BUCKETS; i++) {
        if (private->buckets[i]) {
            used_buckets++;
        }
    }

    snprintf(info, size,
             "Keys: %zu\n"
             "Buckets: %d\n"
             "Used buckets: %zu\n"
             "Load factor: %.2f",
             private->size,
             MAX_BUCKETS,
             used_buckets,
             (float)private->size / MAX_BUCKETS);

    return INFRAX_OK;
}

// Service lifecycle
static infrax_error_t peerx_memkv_init(PeerxMemKV* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Initialize base service
    return PeerxServiceClass.init(&self->base, config);
}

static infrax_error_t peerx_memkv_start(PeerxMemKV* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Start base service
    return PeerxServiceClass.start(&self->base);
}

static infrax_error_t peerx_memkv_stop(PeerxMemKV* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Stop base service
    infrax_error_t err = PeerxServiceClass.stop(&self->base);
    if (err != INFRAX_OK) {
        return err;
    }

    // Flush all data
    return peerx_memkv_flush(self);
}

static infrax_error_t peerx_memkv_reload(PeerxMemKV* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Reload base service
    return PeerxServiceClass.reload(&self->base);
}

// Status and error handling
static infrax_error_t peerx_memkv_get_status(PeerxMemKV* self, char* status, size_t size) {
    if (!self || !status || size == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxMemKVPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Get base status
    char base_status[512];
    infrax_error_t err = PeerxServiceClass.get_status(&self->base, base_status, sizeof(base_status));
    if (err != INFRAX_OK) {
        return err;
    }

    // Add MemKV specific status
    char info[512];
    err = peerx_memkv_info(self, info, sizeof(info));
    if (err != INFRAX_OK) {
        return err;
    }

    snprintf(status, size, "%s\n%s", base_status, info);
    return INFRAX_OK;
}

// Helper functions
void peerx_memkv_value_init(peerx_memkv_value_t* value) {
    if (!value) return;
    memset(value, 0, sizeof(peerx_memkv_value_t));
}

void peerx_memkv_value_free(peerx_memkv_value_t* value) {
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

infrax_error_t peerx_memkv_value_copy(peerx_memkv_value_t* dst,
                                     const peerx_memkv_value_t* src) {
    if (!dst || !src) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    peerx_memkv_value_free(dst);
    dst->type = src->type;

    switch (src->type) {
        case PEERX_MEMKV_TYPE_STRING:
            if (src->value.str) {
                dst->value.str = g_memory->alloc(g_memory, strlen(src->value.str) + 1);
                if (!dst->value.str) {
                    return INFRAX_ERROR_NO_MEMORY;
                }
                strcpy(dst->value.str, src->value.str);
            }
            break;

        case PEERX_MEMKV_TYPE_INT:
            dst->value.i = src->value.i;
            break;

        case PEERX_MEMKV_TYPE_FLOAT:
            dst->value.f = src->value.f;
            break;

        case PEERX_MEMKV_TYPE_BINARY:
            if (src->value.bin.data && src->value.bin.size > 0) {
                dst->value.bin.data = g_memory->alloc(g_memory, src->value.bin.size);
                if (!dst->value.bin.data) {
                    return INFRAX_ERROR_NO_MEMORY;
                }
                memcpy(dst->value.bin.data, src->value.bin.data, src->value.bin.size);
                dst->value.bin.size = src->value.bin.size;
            }
            break;
    }

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

// Global class instance
const PeerxMemKVClassType PeerxMemKVClass = {
    .new = peerx_memkv_new,
    .free = peerx_memkv_free,
    .init = peerx_memkv_init,
    .start = peerx_memkv_start,
    .stop = peerx_memkv_stop,
    .reload = peerx_memkv_reload,
    .get_status = peerx_memkv_get_status,
    .get_error = PeerxServiceClass.get_error,
    .clear_error = PeerxServiceClass.clear_error,
    .validate_config = PeerxServiceClass.validate_config,
    .apply_config = PeerxServiceClass.apply_config
}; 