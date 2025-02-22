#include "PolyxCmdline.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

#define MAX_COMMANDS 32
#define MAX_OPTIONS 16

// Private data structure
typedef struct {
    polyx_cmd_t commands[MAX_COMMANDS];
    InfraxSize command_count;
    InfraxMemory* memory;
    InfraxCore* core;
} PolyxCmdlinePrivate;

// Global instances
static InfraxMemory* g_memory = NULL;
static InfraxCore* g_core = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxCoreClassType InfraxCoreClass;

// Forward declarations
static InfraxBool init_memory(void);

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
    private->core = g_core;
    private->command_count = 0;

    self->private_data = private;
    return self;
}

// Destructor
static void polyx_cmdline_free(PolyxCmdline* self) {
    if (!self) return;

    if (self->private_data) {
        g_memory->dealloc(g_memory, self->private_data);
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
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    if (private->command_count >= MAX_COMMANDS) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Too many commands");
    }

    // Check for duplicate
    for (InfraxSize i = 0; i < private->command_count; i++) {
        if (g_core->strcmp(g_core, private->commands[i].name, cmd->name) == 0) {
            return make_error(INFRAX_ERROR_FILE_EXISTS, "Command already exists");
        }
    }

    // Add command
    g_core->memcpy(g_core, &private->commands[private->command_count], cmd,
                 sizeof(polyx_cmd_t));
    private->command_count++;

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Command execution
static InfraxError polyx_cmdline_execute(PolyxCmdline* self, const polyx_cmd_context_t* ctx,
                                       InfraxI32 argc, char** argv) {
    if (!self || !argv || argc < 1) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxCmdlinePrivate* private = self->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Find command
    const char* cmd_name = argv[0];
    for (InfraxSize i = 0; i < private->command_count; i++) {
        if (g_core->strcmp(g_core, private->commands[i].name, cmd_name) == 0) {
            return private->commands[i].handler(ctx, argc, argv);
        }
    }

    g_core->printf(g_core, "Unknown command: %s\n", cmd_name);
    return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Command not found");
}

// Help and usage
static void polyx_cmdline_print_usage(PolyxCmdline* self, const char* cmd_name) {
    if (!self || !cmd_name) return;

    PolyxCmdlinePrivate* private = self->private_data;
    if (!private) return;

    // Find command
    for (InfraxSize i = 0; i < private->command_count; i++) {
        if (g_core->strcmp(g_core, private->commands[i].name, cmd_name) == 0) {
            const polyx_cmd_t* cmd = &private->commands[i];
            g_core->printf(g_core, "Usage: %s <command> [options]\n", cmd_name);
            g_core->printf(g_core, "\nDescription:\n  %s\n", cmd->desc);
            g_core->printf(g_core, "\nOptions:\n");
            for (InfraxSize j = 0; j < cmd->option_count; j++) {
                g_core->printf(g_core, "  --%s%s\t%s\n",
                    cmd->options[j].name,
                    cmd->options[j].has_value ? "=<value>" : "",
                    cmd->options[j].desc);
            }
            return;
        }
    }

    g_core->printf(g_core, "Unknown command: %s\n", cmd_name);
}

static void polyx_cmdline_print_help(PolyxCmdline* self, const char* cmd_name) {
    if (!self) return;

    PolyxCmdlinePrivate* private = self->private_data;
    if (!private) return;

    if (cmd_name) {
        polyx_cmdline_print_usage(self, cmd_name);
        return;
    }

    g_core->printf(g_core, "Available commands:\n");
    for (InfraxSize i = 0; i < private->command_count; i++) {
        g_core->printf(g_core, "  %-20s %s\n",
            private->commands[i].name,
            private->commands[i].desc);
    }
}

// Memory initialization
static InfraxBool init_memory(void) {
    if (g_memory && g_core) return INFRAX_TRUE;

    // Create memory configuration
    InfraxMemoryConfig mem_config = {
        .initial_size = 1024 * 1024,  // 1MB initial size
        .use_gc = INFRAX_FALSE,       // No GC for now
        .use_pool = INFRAX_TRUE,      // Use memory pool
        .gc_threshold = 0             // Not used when GC is disabled
    };

    // Create memory instance
    g_memory = InfraxMemoryClass.new(&mem_config);
    if (!g_memory) return INFRAX_FALSE;

    // Get core singleton instance
    g_core = InfraxCoreClass.singleton();
    if (!g_core) {
        InfraxMemoryClass.free(g_memory);
        g_memory = NULL;
        return INFRAX_FALSE;
    }

    return INFRAX_TRUE;
}

// Global class instance
const PolyxCmdlineClassType PolyxCmdlineClass = {
    .new = polyx_cmdline_new,
    .free = polyx_cmdline_free,
    .register_cmd = polyx_cmdline_register_cmd,
    .execute = polyx_cmdline_execute,
    .print_usage = polyx_cmdline_print_usage,
    .print_help = polyx_cmdline_print_help
}; 