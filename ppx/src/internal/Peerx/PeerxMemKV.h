#ifndef PEERX_MEMKV_INTERFACE_H
#define PEERX_MEMKV_INTERFACE_H

#include "PeerxService.h"

// Forward declarations
typedef struct PeerxMemKV PeerxMemKV;
typedef struct PeerxMemKVClassType PeerxMemKVClassType;

// Value types
typedef enum {
    PEERX_MEMKV_TYPE_STRING,
    PEERX_MEMKV_TYPE_INT,
    PEERX_MEMKV_TYPE_FLOAT,
    PEERX_MEMKV_TYPE_BINARY
} peerx_memkv_type_t;

// Value structure
typedef struct {
    peerx_memkv_type_t type;
    union {
        char* str;
        int64_t i;
        double f;
        struct {
            void* data;
            size_t size;
        } bin;
    } value;
} peerx_memkv_value_t;

// Key-value pair
typedef struct {
    char key[POLYX_CMD_MAX_NAME];
    peerx_memkv_value_t value;
    int64_t expire_at;  // Unix timestamp in milliseconds, 0 for no expiration
} peerx_memkv_pair_t;

// MemKV service instance
struct PeerxMemKV {
    // Base service
    PeerxService base;
    
    // Key-value operations
    infrax_error_t (*set)(PeerxMemKV* self, const char* key, const peerx_memkv_value_t* value);
    infrax_error_t (*set_ex)(PeerxMemKV* self, const char* key, const peerx_memkv_value_t* value, 
                            int64_t ttl_ms);
    infrax_error_t (*get)(PeerxMemKV* self, const char* key, peerx_memkv_value_t* value);
    infrax_error_t (*del)(PeerxMemKV* self, const char* key);
    bool (*exists)(PeerxMemKV* self, const char* key);
    
    // Batch operations
    infrax_error_t (*multi_set)(PeerxMemKV* self, const peerx_memkv_pair_t* pairs, size_t count);
    infrax_error_t (*multi_get)(PeerxMemKV* self, const char** keys, size_t key_count,
                               peerx_memkv_pair_t* pairs, size_t* pair_count);
    infrax_error_t (*multi_del)(PeerxMemKV* self, const char** keys, size_t count);
    
    // Key operations
    infrax_error_t (*keys)(PeerxMemKV* self, const char* pattern, char** keys, size_t* count);
    infrax_error_t (*expire)(PeerxMemKV* self, const char* key, int64_t ttl_ms);
    infrax_error_t (*ttl)(PeerxMemKV* self, const char* key, int64_t* ttl_ms);
    
    // Server operations
    infrax_error_t (*flush)(PeerxMemKV* self);
    infrax_error_t (*info)(PeerxMemKV* self, char* info, size_t size);
};

// MemKV service class interface
struct PeerxMemKVClassType {
    // Constructor/Destructor
    PeerxMemKV* (*new)(void);
    void (*free)(PeerxMemKV* self);
    
    // Service lifecycle (inherited from PeerxService)
    infrax_error_t (*init)(PeerxMemKV* self, const polyx_service_config_t* config);
    infrax_error_t (*start)(PeerxMemKV* self);
    infrax_error_t (*stop)(PeerxMemKV* self);
    infrax_error_t (*reload)(PeerxMemKV* self);
    
    // Status and error handling (inherited from PeerxService)
    infrax_error_t (*get_status)(PeerxMemKV* self, char* status, size_t size);
    const char* (*get_error)(PeerxMemKV* self);
    void (*clear_error)(PeerxMemKV* self);
    
    // Configuration (inherited from PeerxService)
    infrax_error_t (*validate_config)(PeerxMemKV* self, const polyx_service_config_t* config);
    infrax_error_t (*apply_config)(PeerxMemKV* self, const polyx_service_config_t* config);
};

// Global class instance
extern const PeerxMemKVClassType PeerxMemKVClass;

// Helper functions
void peerx_memkv_value_init(peerx_memkv_value_t* value);
void peerx_memkv_value_free(peerx_memkv_value_t* value);
infrax_error_t peerx_memkv_value_copy(peerx_memkv_value_t* dst, const peerx_memkv_value_t* src);

#endif // PEERX_MEMKV_INTERFACE_H 