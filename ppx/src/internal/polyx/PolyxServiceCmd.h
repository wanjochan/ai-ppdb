#ifndef POLYX_SERVICE_CMD_H
#define POLYX_SERVICE_CMD_H

#include "PolyxCmdline.h"
#include "PolyxService.h"

// Forward declarations
typedef struct PolyxServiceCmd PolyxServiceCmd;
typedef struct PolyxServiceCmdClassType PolyxServiceCmdClassType;

// Service command instance
struct PolyxServiceCmd {
    PolyxServiceCmd* self;
    const PolyxServiceCmdClassType* klass;
    
    // Public members
    PolyxService* service;
    PolyxCmdline* cmdline;
    
    // Private data
    void* private_data;
};

// Service command class interface
struct PolyxServiceCmdClassType {
    // Constructor/Destructor
    PolyxServiceCmd* (*new)(void);
    void (*free)(PolyxServiceCmd* self);
    
    // Service command handlers
    infrax_error_t (*handle_rinetd)(PolyxServiceCmd* self, const polyx_config_t* config, int argc, char** argv);
    infrax_error_t (*handle_sqlite)(PolyxServiceCmd* self, const polyx_config_t* config, int argc, char** argv);
    infrax_error_t (*handle_memkv)(PolyxServiceCmd* self, const polyx_config_t* config, int argc, char** argv);
    
    // Service command registration
    infrax_error_t (*register_all)(PolyxServiceCmd* self);
};

// Global class instance
extern const PolyxServiceCmdClassType PolyxServiceCmdClass;

#endif // POLYX_SERVICE_CMD_H 