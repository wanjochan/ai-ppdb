#ifndef POLYX_SERVICE_H
#define POLYX_SERVICE_H

#include "PolyxConfig.h"
#include "internal/infrax/InfraxCore.h"

// Forward declarations
typedef struct PolyxService PolyxService;
typedef struct PolyxServiceClassType PolyxServiceClassType;

// Service states
typedef enum {
    POLYX_SERVICE_STATE_INIT,
    POLYX_SERVICE_STATE_READY,
    POLYX_SERVICE_STATE_RUNNING,
    POLYX_SERVICE_STATE_STOPPED,
    POLYX_SERVICE_STATE_ERROR
} polyx_service_state_t;

// Service instance
struct PolyxService {
    PolyxService* self;
    const PolyxServiceClassType* klass;
    
    // Public members
    polyx_service_config_t config;
    polyx_service_state_t state;
    
    // Service operations
    infrax_error_t (*init)(struct PolyxService* self);
    infrax_error_t (*cleanup)(struct PolyxService* self);
    infrax_error_t (*start)(struct PolyxService* self);
    infrax_error_t (*stop)(struct PolyxService* self);
    infrax_error_t (*reload)(struct PolyxService* self);
    infrax_error_t (*get_status)(struct PolyxService* self, char* status, size_t size);
    
    // Private data
    void* private_data;
};

// Service class interface
struct PolyxServiceClassType {
    // Constructor/Destructor
    PolyxService* (*new)(void);
    void (*free)(PolyxService* self);
    
    // Service registration
    infrax_error_t (*register_service)(PolyxService* self, const polyx_service_config_t* config);
    PolyxService* (*get_service)(PolyxService* self, polyx_service_type_t type);
    
    // Service lifecycle management
    infrax_error_t (*start_all)(PolyxService* self);
    infrax_error_t (*stop_all)(PolyxService* self);
    infrax_error_t (*reload_all)(PolyxService* self);
    
    // Service status
    infrax_error_t (*get_status)(PolyxService* self, polyx_service_type_t type, char* status, size_t size);
    infrax_error_t (*get_all_status)(PolyxService* self, char* status, size_t size);
};

// Global class instance
extern const PolyxServiceClassType PolyxServiceClass;

#endif // POLYX_SERVICE_H 