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
    bool has_value;
} polyx_cmd_arg_t;

// Service configuration
typedef struct {
    polyx_service_type_t type;
    char listen_host[POLYX_CMD_MAX_NAME];
    int listen_port;
    char target_host[POLYX_CMD_MAX_NAME];
    int target_port;
    char backend[POLYX_CMD_MAX_VALUE];
} polyx_service_config_t;

// Global configuration
typedef struct {
    polyx_cmd_arg_t args[POLYX_CMD_MAX_ARGS];
    int arg_count;
    int log_level;
    polyx_service_config_t services[POLYX_CMD_MAX_SERVICES];
    int service_count;
} polyx_config_t;

// Command option
typedef struct {
    char name[POLYX_CMD_MAX_NAME];
    char desc[POLYX_CMD_MAX_DESC];
    bool has_value;
} polyx_cmd_option_t;

// Command structure
typedef struct {
    char name[POLYX_CMD_MAX_NAME];
    char desc[POLYX_CMD_MAX_DESC];
    const polyx_cmd_option_t* options;
    int option_count;
    infrax_error_t (*handler)(const polyx_config_t* config, int argc, char** argv);
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
    infrax_error_t (*register_cmd)(PolyxCmdline* self, const polyx_cmd_t* cmd);
    
    // Argument parsing
    infrax_error_t (*parse_args)(PolyxCmdline* self, int argc, char** argv);
    
    // Option handling
    infrax_error_t (*get_option)(PolyxCmdline* self, const char* option, char* value, size_t size);
    bool (*has_option)(PolyxCmdline* self, const char* option);
    infrax_error_t (*get_int_option)(PolyxCmdline* self, const char* option, int* value);
    
    // Command lookup
    const polyx_cmd_t* (*find_command)(PolyxCmdline* self, const char* name);
    const polyx_cmd_t* (*get_commands)(PolyxCmdline* self);
    size_t (*get_command_count)(PolyxCmdline* self);
};

// Global class instance
extern const PolyxCmdlineClassType PolyxCmdlineClass;

#endif // POLYX_CMDLINE_H 