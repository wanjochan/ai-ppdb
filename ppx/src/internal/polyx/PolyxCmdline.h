#ifndef POLYX_CMDLINE_H
#define POLYX_CMDLINE_H

#include "internal/infrax/InfraxCore.h"

// Forward declarations
typedef struct PolyxCmdline PolyxCmdline;
typedef struct PolyxCmdlineClassType PolyxCmdlineClassType;

// Constants
#define POLYX_CMD_MAX_NAME 32
#define POLYX_CMD_MAX_DESC 256
#define POLYX_CMD_MAX_ARGS 16
#define POLYX_CMD_MAX_VALUE 1024
#define POLYX_CMD_MAX_SERVICES 32

// Service types
typedef enum {
    POLYX_SERVICE_RINETD,
    POLYX_SERVICE_SQLITE,
    POLYX_SERVICE_MEMKV,
    POLYX_SERVICE_DISKV
} polyx_service_type_t;

// Command line argument
typedef struct {
    char name[POLYX_CMD_MAX_NAME];
    char value[POLYX_CMD_MAX_VALUE];
    InfraxBool has_value;
} polyx_cmd_arg_t;

// Service configuration
typedef struct {
    polyx_service_type_t type;
    char listen_host[POLYX_CMD_MAX_NAME];
    InfraxI32 listen_port;
    char target_host[POLYX_CMD_MAX_NAME];
    InfraxI32 target_port;
    char backend[POLYX_CMD_MAX_VALUE];
} polyx_service_config_t;

// Global configuration
typedef struct {
    polyx_cmd_arg_t args[POLYX_CMD_MAX_ARGS];
    InfraxI32 arg_count;
    InfraxI32 log_level;
    polyx_service_config_t services[POLYX_CMD_MAX_SERVICES];
    InfraxI32 service_count;
} polyx_config_t;

// Command option
typedef struct {
    char name[POLYX_CMD_MAX_NAME];
    char desc[POLYX_CMD_MAX_DESC];
    InfraxBool has_value;
} polyx_cmd_option_t;

// Command structure
typedef struct {
    char name[POLYX_CMD_MAX_NAME];
    char desc[POLYX_CMD_MAX_DESC];
    const polyx_cmd_option_t* options;
    InfraxI32 option_count;
    InfraxError (*handler)(const polyx_config_t* config, InfraxI32 argc, char** argv);
} polyx_cmd_t;

// Command line class
struct PolyxCmdline {
    PolyxCmdline* self;
    const PolyxCmdlineClassType* klass;
    
    // Public members
    polyx_config_t config;
    
    // Private data
    void* private_data;
};

// Command line class interface
struct PolyxCmdlineClassType {
    // Constructor/Destructor
    PolyxCmdline* (*new)(void);
    void (*free)(PolyxCmdline* self);
    
    // Command registration
    InfraxError (*register_cmd)(PolyxCmdline* self, const polyx_cmd_t* cmd);
    
    // Argument parsing
    InfraxError (*parse_args)(PolyxCmdline* self, InfraxI32 argc, char** argv);
    
    // Option handling
    InfraxError (*get_option)(PolyxCmdline* self, const char* option, char* value, InfraxSize size);
    InfraxBool (*has_option)(PolyxCmdline* self, const char* option);
    InfraxError (*get_int_option)(PolyxCmdline* self, const char* option, InfraxI32* value);
    
    // Command lookup
    const polyx_cmd_t* (*find_command)(PolyxCmdline* self, const char* name);
    const polyx_cmd_t* (*get_commands)(PolyxCmdline* self);
    InfraxSize (*get_command_count)(PolyxCmdline* self);
};

// Global class instance
extern const PolyxCmdlineClassType PolyxCmdlineClass;

#endif // POLYX_CMDLINE_H 