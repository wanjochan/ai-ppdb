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
    InfraxError (*init)(struct PolyxService* self);
    InfraxError (*cleanup)(struct PolyxService* self);
    InfraxError (*start)(struct PolyxService* self);
    InfraxError (*stop)(struct PolyxService* self);
    InfraxError (*reload)(struct PolyxService* self);
    InfraxError (*get_status)(struct PolyxService* self, char* status, InfraxSize size);
    
    // Private data
    void* private_data;
};

// Service class interface
struct PolyxServiceClassType {
    // Constructor/Destructor
    PolyxService* (*new)(void);
    void (*free)(PolyxService* self);
    
    // Service registration
    InfraxError (*register_service)(PolyxService* self, const polyx_service_config_t* config);
    PolyxService* (*get_service)(PolyxService* self, polyx_service_type_t type);
    
    // Service lifecycle management
    InfraxError (*start_all)(PolyxService* self);
    InfraxError (*stop_all)(PolyxService* self);
    InfraxError (*reload_all)(PolyxService* self);
    
    // Service status
    InfraxError (*get_status)(PolyxService* self, polyx_service_type_t type, char* status, InfraxSize size);
    InfraxError (*get_all_status)(PolyxService* self, char* status, InfraxSize size);
};

// Global class instance
extern const PolyxServiceClassType PolyxServiceClass;

#endif // POLYX_SERVICE_H 