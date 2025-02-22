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

// Command line argument
typedef struct {
    char name[POLYX_CMD_MAX_NAME];
    char value[POLYX_CMD_MAX_VALUE];
    InfraxBool has_value;
} polyx_cmd_arg_t;

// Command context
typedef struct {
    polyx_cmd_arg_t args[POLYX_CMD_MAX_ARGS];
    InfraxI32 arg_count;
    InfraxI32 log_level;
    void* user_data;
} polyx_cmd_context_t;

// Command option flags
typedef enum {
    POLYX_CMD_OPTION_NONE = 0,
    POLYX_CMD_OPTION_REQUIRED = 1,
    POLYX_CMD_OPTION_OPTIONAL = 2
} polyx_cmd_option_flags_t;

// Command option
typedef struct {
    const char* name;
    const char* desc;
    InfraxBool has_value;
} polyx_cmd_option_t;

// Command handler function type
typedef InfraxError (*polyx_cmd_handler_t)(const polyx_cmd_context_t* ctx, InfraxI32 argc, char** argv);

// Command definition
typedef struct {
    const char* name;
    const char* desc;
    const polyx_cmd_option_t* options;
    InfraxSize option_count;
    polyx_cmd_handler_t handler;
} polyx_cmd_t;

// Command line instance
struct PolyxCmdline {
    PolyxCmdline* self;
    const PolyxCmdlineClassType* klass;
    
    // Command registration
    InfraxError (*register_cmd)(struct PolyxCmdline* self, const polyx_cmd_t* cmd);
    
    // Command execution
    InfraxError (*execute)(struct PolyxCmdline* self, const polyx_cmd_context_t* ctx,
                          InfraxI32 argc, char** argv);
    
    // Help and usage
    void (*print_usage)(struct PolyxCmdline* self, const char* cmd_name);
    void (*print_help)(struct PolyxCmdline* self, const char* cmd_name);
    
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
    
    // Command execution
    InfraxError (*execute)(PolyxCmdline* self, const polyx_cmd_context_t* ctx,
                          InfraxI32 argc, char** argv);
    
    // Help and usage
    void (*print_usage)(PolyxCmdline* self, const char* cmd_name);
    void (*print_help)(PolyxCmdline* self, const char* cmd_name);
};

// Global class instance
extern const PolyxCmdlineClassType PolyxCmdlineClass;

#endif // POLYX_CMDLINE_H 