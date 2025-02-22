#ifndef POLYX_SERVICE_H
#define POLYX_SERVICE_H

#include "PolyxConfig.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxError.h"

// Forward declarations
typedef struct PolyxService PolyxService;
typedef struct PolyxServiceClassType PolyxServiceClassType;

// Service types
typedef enum {
    POLYX_SERVICE_RINETD = 1,
    POLYX_SERVICE_SQLITE = 2,
    POLYX_SERVICE_MEMKV = 3
} polyx_service_type_t;

// Service states
typedef enum {
    POLYX_SERVICE_STATE_INIT = 0,
    POLYX_SERVICE_STATE_READY = 1,
    POLYX_SERVICE_STATE_RUNNING = 2,
    POLYX_SERVICE_STATE_STOPPED = 3,
    POLYX_SERVICE_STATE_ERROR = 4
} polyx_service_state_t;

// Service configuration
typedef struct {
    polyx_service_type_t type;
    char name[64];
    void* user_data;
} polyx_service_config_t;

// Service factory function type
typedef PolyxService* (*polyx_service_factory_t)(void);

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
    
    // Status and error handling
    const char* (*get_error)(PolyxService* self);
    void (*clear_error)(PolyxService* self);
    
    // Configuration
    InfraxError (*validate_config)(PolyxService* self, const polyx_service_config_t* config);
    InfraxError (*apply_config)(PolyxService* self, const polyx_service_config_t* config);
    
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
    InfraxError (*register_factory)(PolyxService* self, polyx_service_type_t type, polyx_service_factory_t factory);
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

// Helper functions
const char* polyx_config_get_service_type_name(polyx_service_type_t type);

#endif // POLYX_SERVICE_H 