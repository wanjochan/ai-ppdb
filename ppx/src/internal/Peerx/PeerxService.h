#ifndef PEERX_SERVICE_H
#define PEERX_SERVICE_H

#include "internal/polyx/PolyxService.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxLog.h"

// Forward declarations
typedef struct PeerxService PeerxService;
typedef struct PeerxServiceClassType PeerxServiceClassType;

// Service instance
struct PeerxService {
    PeerxService* self;
    const PeerxServiceClassType* klass;
    
    // Base service
    PolyxService base;
    
    // Async support
    InfraxAsync* async;
    
    // Service state
    bool is_initialized;
    bool is_running;
    char* error_message;
    
    // Private data
    void* private_data;
};

// Service class interface
struct PeerxServiceClassType {
    // Constructor/Destructor
    PeerxService* (*new)(void);
    void (*free)(PeerxService* self);
    
    // Service lifecycle
    infrax_error_t (*init)(PeerxService* self, const polyx_service_config_t* config);
    infrax_error_t (*start)(PeerxService* self);
    infrax_error_t (*stop)(PeerxService* self);
    infrax_error_t (*reload)(PeerxService* self);
    
    // Status and error handling
    infrax_error_t (*get_status)(PeerxService* self, char* status, size_t size);
    const char* (*get_error)(PeerxService* self);
    void (*clear_error)(PeerxService* self);
    
    // Configuration
    infrax_error_t (*validate_config)(PeerxService* self, const polyx_service_config_t* config);
    infrax_error_t (*apply_config)(PeerxService* self, const polyx_service_config_t* config);
};

// Global class instance
extern const PeerxServiceClassType PeerxServiceClass;

// Helper macros
#define PEERX_SERVICE_ERROR(self, fmt, ...) do { \
    if (self->error_message) { \
        free(self->error_message); \
    } \
    asprintf(&self->error_message, fmt, ##__VA_ARGS__); \
    infrax_log_error(fmt, ##__VA_ARGS__); \
} while(0)

#define PEERX_SERVICE_INFO(fmt, ...) \
    infrax_log_info(fmt, ##__VA_ARGS__)

#define PEERX_SERVICE_DEBUG(fmt, ...) \
    infrax_log_debug(fmt, ##__VA_ARGS__)

#endif // PEERX_SERVICE_H 