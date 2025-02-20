#ifndef PEERX_SERVICE_H
#define PEERX_SERVICE_H

#include "internal/polyx/PolyxService.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxLog.h"

// Forward declarations
typedef struct PeerxService PeerxService;
typedef struct PeerxServiceClassType PeerxServiceClassType;

// Error codes
#define INFRAX_ERROR_INVALID_STATE INFRAX_ERROR_INVALID_PARAM

// Service instance
struct PeerxService {
    PeerxService* self;
    const PeerxServiceClassType* klass;
    
    // Base service
    PolyxService base;
    
    // Async support
    InfraxAsync* async;
    
    // Service state
    InfraxBool is_initialized;
    InfraxBool is_running;
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
    InfraxError (*init)(PeerxService* self, const polyx_service_config_t* config);
    InfraxError (*start)(PeerxService* self);
    InfraxError (*stop)(PeerxService* self);
    InfraxError (*reload)(PeerxService* self);
    
    // Status and error handling
    InfraxError (*get_status)(PeerxService* self, char* status, size_t size);
    const char* (*get_error)(PeerxService* self);
    void (*clear_error)(PeerxService* self);
    
    // Configuration
    InfraxError (*validate_config)(PeerxService* self, const polyx_service_config_t* config);
    InfraxError (*apply_config)(PeerxService* self, const polyx_service_config_t* config);
};

// Global class instance
extern const PeerxServiceClassType PeerxServiceClass;

// Macro for error logging
#define PEERX_SERVICE_ERROR(self, fmt, ...) do { \
    InfraxLog* log = InfraxLogClass.singleton(); \
    if (log != NULL) { \
        log->error(log, fmt, ##__VA_ARGS__); \
    } \
    if ((self) != NULL && (self)->error_message == NULL) { \
        size_t size = 256; \
        InfraxCore* core = InfraxCoreClass.singleton(); \
        if (core != NULL) { \
            (self)->error_message = core->malloc(core, size); \
            if ((self)->error_message != NULL) { \
                core->snprintf(core, (self)->error_message, size, fmt, ##__VA_ARGS__); \
            } \
        } \
    } \
} while(0)

#endif // PEERX_SERVICE_H 