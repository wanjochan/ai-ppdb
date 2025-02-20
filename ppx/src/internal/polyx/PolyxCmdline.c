#include "PolyxCmdline.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxCore.h"

// Private data structure
typedef struct {
    polyx_cmd_t* commands;
    InfraxSize command_count;
    InfraxSize command_capacity;
    InfraxMemory* memory;
} PolyxCmdlinePrivate;

// Forward declarations of private functions
static void trim_string(char* str);
static InfraxError parse_option(const char* arg, polyx_cmd_arg_t* cmd_arg);
static InfraxBool init_memory(void);
static InfraxI32 string_to_int(InfraxCore* core, const char* str);

// Global memory manager and core
static InfraxMemory* g_memory = NULL;
static InfraxCore* g_core = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxCoreClassType InfraxCoreClass;

// Constructor
static PolyxCmdline* polyx_cmdline_new(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Get core singleton
    g_core = InfraxCoreClass.singleton();
    if (!g_core) {
        return NULL;
    }

    // Allocate instance
    PolyxCmdline* self = g_memory->alloc(g_memory, sizeof(PolyxCmdline));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    g_core->memset(g_core, self, 0, sizeof(PolyxCmdline));
    self->self = self;
    self->klass = &PolyxCmdlineClass;

    // Allocate private data
    PolyxCmdlinePrivate* private = g_memory->alloc(g_memory, sizeof(PolyxCmdlinePrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    g_core->memset(g_core, private, 0, sizeof(PolyxCmdlinePrivate));
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
static InfraxError polyx_cmdline_register_cmd(PolyxCmdline* self, const polyx_cmd_t* cmd) {
    if (!self || !cmd) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxCmdlinePrivate* private = self->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid state");
    }

    // Check for duplicate
    for (InfraxSize i = 0; i < private->command_count; i++) {
        if (g_core->strcmp(g_core, private->commands[i].name, cmd->name) == 0) {
            return make_error(INFRAX_ERROR_FILE_EXISTS, "Command already exists");
        }
    }

    // Expand array if needed
    if (private->command_count >= private->command_capacity) {
        InfraxSize new_capacity = private->command_capacity * 2;
        polyx_cmd_t* new_commands = private->memory->alloc(private->memory, 
                                                         new_capacity * sizeof(polyx_cmd_t));
        if (!new_commands) {
            return make_error(INFRAX_ERROR_NO_MEMORY, "Memory allocation failed");
        }

        g_core->memcpy(g_core, new_commands, private->commands, 
                        private->command_count * sizeof(polyx_cmd_t));
        private->memory->dealloc(private->memory, private->commands);
        private->commands = new_commands;
        private->command_capacity = new_capacity;
    }

    // Add command
    g_core->memcpy(g_core, &private->commands[private->command_count++], cmd, sizeof(polyx_cmd_t));
    return make_error(INFRAX_ERROR_OK, NULL);
}

// Argument parsing
static InfraxError polyx_cmdline_parse_args(PolyxCmdline* self, InfraxI32 argc, char** argv) {
    if (!self || argc < 1 || !argv) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Reset config
    g_core->memset(g_core, &self->config, 0, sizeof(polyx_config_t));

    // Parse arguments
    for (InfraxI32 i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            polyx_cmd_arg_t arg = {0};
            InfraxError err = parse_option(argv[i], &arg);
            if (!INFRAX_ERROR_IS_OK(err)) {
                return err;
            }

            // Check for value in next argument
            if (arg.has_value && i + 1 < argc && argv[i + 1][0] != '-') {
                g_core->strncpy(g_core, arg.value, argv[++i], POLYX_CMD_MAX_VALUE - 1);
            }

            // Add to config
            if (self->config.arg_count < POLYX_CMD_MAX_ARGS) {
                g_core->memcpy(g_core, &self->config.args[self->config.arg_count++], 
                               &arg, sizeof(polyx_cmd_arg_t));
            }
        }
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Option handling
static InfraxError polyx_cmdline_get_option(PolyxCmdline* self, const char* option, 
                                          char* value, InfraxSize size) {
    if (!self || !option || !value || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Skip leading dashes
    while (*option == '-') option++;

    // Find option
    for (InfraxI32 i = 0; i < self->config.arg_count; i++) {
        if (g_core->strcmp(g_core, self->config.args[i].name, option) == 0) {
            if (self->config.args[i].has_value) {
                g_core->strncpy(g_core, value, self->config.args[i].value, size - 1);
                value[size - 1] = '\0';
                return make_error(INFRAX_ERROR_OK, NULL);
            }
            return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Option has no value");
        }
    }

    return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Option not found");
}

static InfraxBool polyx_cmdline_has_option(PolyxCmdline* self, const char* option) {
    if (!self || !option) {
        return INFRAX_FALSE;
    }

    // Skip leading dashes
    while (*option == '-') option++;

    // Find option
    for (InfraxI32 i = 0; i < self->config.arg_count; i++) {
        if (g_core->strcmp(g_core, self->config.args[i].name, option) == 0) {
            return INFRAX_TRUE;
        }
    }

    return INFRAX_FALSE;
}

static InfraxError polyx_cmdline_get_int_option(PolyxCmdline* self, const char* option, 
                                              InfraxI32* value) {
    if (!self || !option || !value) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    char str_value[POLYX_CMD_MAX_VALUE];
    InfraxError err = polyx_cmdline_get_option(self, option, str_value, sizeof(str_value));
    if (!INFRAX_ERROR_IS_OK(err)) {
        return err;
    }

    *value = string_to_int(g_core, str_value);
    return make_error(INFRAX_ERROR_OK, NULL);
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

    for (InfraxSize i = 0; i < private->command_count; i++) {
        if (g_core->strcmp(g_core, private->commands[i].name, name) == 0) {
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

static InfraxSize polyx_cmdline_get_command_count(PolyxCmdline* self) {
    if (!self) {
        return 0;
    }

    PolyxCmdlinePrivate* private = self->private_data;
    return private ? private->command_count : 0;
}

// Private helper functions
static void trim_string(char* str) {
    if (!str) return;

    // Trim leading whitespace
    char* start = str;
    while (*start && g_core->isspace(g_core, *start)) start++;
    
    if (start != str) {
        g_core->memmove(g_core, str, start, g_core->strlen(g_core, start) + 1);
    }

    // Trim trailing whitespace
    char* end = str + g_core->strlen(g_core, str) - 1;
    while (end > str && g_core->isspace(g_core, *end)) end--;
    *(end + 1) = '\0';
}

static InfraxError parse_option(const char* arg, polyx_cmd_arg_t* cmd_arg) {
    if (!arg || !cmd_arg) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Skip leading dashes
    while (*arg == '-') arg++;

    // Find value separator
    const char* value_sep = g_core->strchr(g_core, arg, '=');
    InfraxSize name_len = value_sep ? (InfraxSize)(value_sep - arg) : g_core->strlen(g_core, arg);

    if (name_len >= POLYX_CMD_MAX_NAME) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Option name too long");
    }

    // Copy name
    g_core->strncpy(g_core, cmd_arg->name, arg, name_len);
    cmd_arg->name[name_len] = '\0';

    // Copy value if present
    if (value_sep) {
        value_sep++; // Skip '='
        g_core->strncpy(g_core, cmd_arg->value, value_sep, POLYX_CMD_MAX_VALUE - 1);
        cmd_arg->has_value = INFRAX_TRUE;
    } else {
        g_core->strncpy(g_core, cmd_arg->name, arg, POLYX_CMD_MAX_NAME - 1);
        cmd_arg->has_value = INFRAX_FALSE;
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxBool init_memory(void) {
    if (g_memory) {
        return INFRAX_TRUE;
    }

    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024 * 10, // 10MB
        .use_gc = INFRAX_FALSE,
        .use_pool = INFRAX_TRUE,
        .gc_threshold = 0
    };

    g_memory = InfraxMemoryClass.new(&config);
    return g_memory != NULL;
}

// String to integer conversion
static InfraxI32 string_to_int(InfraxCore* core, const char* str) {
    if (!core || !str) return 0;

    InfraxI32 result = 0;
    InfraxI32 sign = 1;
    
    // Skip whitespace
    while (*str && core->isspace(core, *str)) str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (*str && core->isdigit(core, *str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
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