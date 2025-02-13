#include "PolyxCmdline.h"
#include "internal/infrax/InfraxMemory.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Private data structure
typedef struct {
    polyx_cmd_t* commands;
    size_t command_count;
    size_t command_capacity;
    InfraxMemory* memory;
} PolyxCmdlinePrivate;

// Forward declarations of private functions
static void trim_string(char* str);
static infrax_error_t parse_option(const char* arg, polyx_cmd_arg_t* cmd_arg);
static bool init_memory(void);

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;

// Constructor
static PolyxCmdline* polyx_cmdline_new(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Allocate instance
    PolyxCmdline* self = g_memory->alloc(g_memory, sizeof(PolyxCmdline));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    memset(self, 0, sizeof(PolyxCmdline));
    self->self = self;
    self->klass = &PolyxCmdlineClass;

    // Allocate private data
    PolyxCmdlinePrivate* private = g_memory->alloc(g_memory, sizeof(PolyxCmdlinePrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    memset(private, 0, sizeof(PolyxCmdlinePrivate));
    private->memory = g_memory;
    private->command_capacity = 16;
    private->commands = g_memory->alloc(g_memory, private->command_capacity * sizeof(polyx_cmd_t));
    if (!private->commands) {
        g_memory->dealloc(g_memory, private);
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    self->private_data = private;
    return self;
}

// Destructor
static void polyx_cmdline_free(PolyxCmdline* self) {
    if (!self) return;

    PolyxCmdlinePrivate* private = self->private_data;
    if (private) {
        if (private->commands) {
            private->memory->dealloc(private->memory, private->commands);
        }
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Command registration
static infrax_error_t polyx_cmdline_register_cmd(PolyxCmdline* self, const polyx_cmd_t* cmd) {
    if (!self || !cmd) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PolyxCmdlinePrivate* private = self->private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Check for duplicate
    for (size_t i = 0; i < private->command_count; i++) {
        if (strcmp(private->commands[i].name, cmd->name) == 0) {
            return INFRAX_ERROR_EXISTS;
        }
    }

    // Expand array if needed
    if (private->command_count >= private->command_capacity) {
        size_t new_capacity = private->command_capacity * 2;
        polyx_cmd_t* new_commands = private->memory->alloc(private->memory, 
                                                         new_capacity * sizeof(polyx_cmd_t));
        if (!new_commands) {
            return INFRAX_ERROR_NO_MEMORY;
        }

        memcpy(new_commands, private->commands, private->command_count * sizeof(polyx_cmd_t));
        private->memory->dealloc(private->memory, private->commands);
        private->commands = new_commands;
        private->command_capacity = new_capacity;
    }

    // Add command
    memcpy(&private->commands[private->command_count++], cmd, sizeof(polyx_cmd_t));
    return INFRAX_OK;
}

// Argument parsing
static infrax_error_t polyx_cmdline_parse_args(PolyxCmdline* self, int argc, char** argv) {
    if (!self || argc < 1 || !argv) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Reset config
    memset(&self->config, 0, sizeof(polyx_config_t));

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            polyx_cmd_arg_t arg = {0};
            infrax_error_t err = parse_option(argv[i], &arg);
            if (err != INFRAX_OK) {
                return err;
            }

            // Check for value in next argument
            if (arg.has_value && i + 1 < argc && argv[i + 1][0] != '-') {
                strncpy(arg.value, argv[++i], POLYX_CMD_MAX_VALUE - 1);
            }

            // Add to config
            if (self->config.arg_count < POLYX_CMD_MAX_ARGS) {
                memcpy(&self->config.args[self->config.arg_count++], &arg, sizeof(polyx_cmd_arg_t));
            }
        }
    }

    return INFRAX_OK;
}

// Option handling
static infrax_error_t polyx_cmdline_get_option(PolyxCmdline* self, const char* option, 
                                             char* value, size_t size) {
    if (!self || !option || !value || size == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Skip leading dashes
    while (*option == '-') option++;

    // Find option
    for (int i = 0; i < self->config.arg_count; i++) {
        if (strcmp(self->config.args[i].name, option) == 0) {
            if (self->config.args[i].has_value) {
                strncpy(value, self->config.args[i].value, size - 1);
                value[size - 1] = '\0';
                return INFRAX_OK;
            }
            return INFRAX_ERROR_NOT_FOUND;
        }
    }

    return INFRAX_ERROR_NOT_FOUND;
}

static bool polyx_cmdline_has_option(PolyxCmdline* self, const char* option) {
    if (!self || !option) {
        return false;
    }

    // Skip leading dashes
    while (*option == '-') option++;

    // Find option
    for (int i = 0; i < self->config.arg_count; i++) {
        if (strcmp(self->config.args[i].name, option) == 0) {
            return true;
        }
    }

    return false;
}

static infrax_error_t polyx_cmdline_get_int_option(PolyxCmdline* self, const char* option, 
                                                 int* value) {
    if (!self || !option || !value) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    char str_value[POLYX_CMD_MAX_VALUE];
    infrax_error_t err = polyx_cmdline_get_option(self, option, str_value, sizeof(str_value));
    if (err != INFRAX_OK) {
        return err;
    }

    char* end;
    *value = (int)strtol(str_value, &end, 10);
    if (*end != '\0') {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    return INFRAX_OK;
}

// Command lookup
static const polyx_cmd_t* polyx_cmdline_find_command(PolyxCmdline* self, const char* name) {
    if (!self || !name) {
        return NULL;
    }

    PolyxCmdlinePrivate* private = self->private_data;
    if (!private) {
        return NULL;
    }

    for (size_t i = 0; i < private->command_count; i++) {
        if (strcmp(private->commands[i].name, name) == 0) {
            return &private->commands[i];
        }
    }

    return NULL;
}

static const polyx_cmd_t* polyx_cmdline_get_commands(PolyxCmdline* self) {
    if (!self) {
        return NULL;
    }

    PolyxCmdlinePrivate* private = self->private_data;
    return private ? private->commands : NULL;
}

static size_t polyx_cmdline_get_command_count(PolyxCmdline* self) {
    if (!self) {
        return 0;
    }

    PolyxCmdlinePrivate* private = self->private_data;
    return private ? private->command_count : 0;
}

// Private helper functions
static void trim_string(char* str) {
    if (!str) return;
    
    char* start = str;
    char* end;
    
    while (isspace(*start)) start++;
    
    if (*start == 0) {
        *str = 0;
        return;
    }
    
    end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    
    *(end + 1) = 0;
    
    if (start != str) {
        memmove(str, start, end - start + 2);
    }
}

static infrax_error_t parse_option(const char* arg, polyx_cmd_arg_t* cmd_arg) {
    if (!arg || !cmd_arg || arg[0] != '-') {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Skip leading dashes
    const char* name = arg;
    while (*name == '-') name++;

    // Check for empty name
    if (!*name) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Copy name
    const char* value = strchr(name, '=');
    if (value) {
        size_t name_len = value - name;
        if (name_len >= POLYX_CMD_MAX_NAME) {
            return INFRAX_ERROR_INVALID_PARAM;
        }
        strncpy(cmd_arg->name, name, name_len);
        cmd_arg->name[name_len] = '\0';
        
        value++; // Skip '='
        strncpy(cmd_arg->value, value, POLYX_CMD_MAX_VALUE - 1);
        cmd_arg->has_value = true;
    } else {
        strncpy(cmd_arg->name, name, POLYX_CMD_MAX_NAME - 1);
        cmd_arg->has_value = false;
    }

    return INFRAX_OK;
}

static bool init_memory(void) {
    if (g_memory) return true;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    return g_memory != NULL;
}

// Global class instance
const PolyxCmdlineClassType PolyxCmdlineClass = {
    .new = polyx_cmdline_new,
    .free = polyx_cmdline_free,
    .register_cmd = polyx_cmdline_register_cmd,
    .parse_args = polyx_cmdline_parse_args,
    .get_option = polyx_cmdline_get_option,
    .has_option = polyx_cmdline_has_option,
    .get_int_option = polyx_cmdline_get_int_option,
    .find_command = polyx_cmdline_find_command,
    .get_commands = polyx_cmdline_get_commands,
    .get_command_count = polyx_cmdline_get_command_count
}; 
} 