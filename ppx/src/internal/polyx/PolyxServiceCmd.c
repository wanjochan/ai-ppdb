#include "PolyxServiceCmd.h"
#include "PolyxConfig.h"
#include "internal/infrax/InfraxMemory.h"
#include <string.h>
#include <stdio.h>

// Common service command options
static const polyx_cmd_option_t service_options[] = {
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show service status", false},
    {"reload", "Reload service configuration", false},
    {"config", "Configuration file path", true},
    {"daemon", "Run as daemon", false}
};

// Private data structure
typedef struct {
    InfraxMemory* memory;
} PolyxServiceCmdPrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;

// Forward declarations of private functions
static bool init_memory(void);
static infrax_error_t handle_service_command(PolyxServiceCmd* self, PolyxService* service, 
                                           const polyx_config_t* config,
                                           const char* config_file);

// Constructor
static PolyxServiceCmd* polyx_service_cmd_new(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Allocate instance
    PolyxServiceCmd* self = g_memory->alloc(g_memory, sizeof(PolyxServiceCmd));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    memset(self, 0, sizeof(PolyxServiceCmd));
    self->self = self;
    self->klass = &PolyxServiceCmdClass;

    // Create service and cmdline instances
    self->service = PolyxServiceClass.new();
    if (!self->service) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    self->cmdline = PolyxCmdlineClass.new();
    if (!self->cmdline) {
        PolyxServiceClass.free(self->service);
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Allocate private data
    PolyxServiceCmdPrivate* private = g_memory->alloc(g_memory, sizeof(PolyxServiceCmdPrivate));
    if (!private) {
        PolyxCmdlineClass.free(self->cmdline);
        PolyxServiceClass.free(self->service);
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    memset(private, 0, sizeof(PolyxServiceCmdPrivate));
    private->memory = g_memory;

    self->private_data = private;
    return self;
}

// Destructor
static void polyx_service_cmd_free(PolyxServiceCmd* self) {
    if (!self) return;

    if (self->cmdline) {
        PolyxCmdlineClass.free(self->cmdline);
    }

    if (self->service) {
        PolyxServiceClass.free(self->service);
    }

    PolyxServiceCmdPrivate* private = self->private_data;
    if (private) {
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Helper functions
static infrax_error_t handle_service_command(PolyxServiceCmd* self, PolyxService* service, 
                                           const polyx_config_t* config,
                                           const char* config_file) {
    if (!self || !service || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Parse configuration file if provided
    if (config_file) {
        polyx_config_t service_config = {0};
        infrax_error_t err = polyx_config_parse_file(config_file, &service_config);
        if (err != INFRAX_OK) {
            fprintf(stderr, "Failed to parse config file: %s\n", config_file);
            return err;
        }

        // Apply configuration
        err = PolyxServiceClass.register_service(self->service, &service_config.services[0]);
        if (err != INFRAX_OK) {
            return err;
        }
    }

    // Handle command
    if (PolyxCmdlineClass.has_option(self->cmdline, "--start")) {
        return service->start(service);
    }
    else if (PolyxCmdlineClass.has_option(self->cmdline, "--stop")) {
        return service->stop(service);
    }
    else if (PolyxCmdlineClass.has_option(self->cmdline, "--reload")) {
        return service->reload(service);
    }
    else if (PolyxCmdlineClass.has_option(self->cmdline, "--status")) {
        char status[1024];
        infrax_error_t err = service->get_status(service, status, sizeof(status));
        if (err == INFRAX_OK) {
            printf("%s\n", status);
        }
        return err;
    }
    else {
        // Default to status
        char status[1024];
        infrax_error_t err = service->get_status(service, status, sizeof(status));
        if (err == INFRAX_OK) {
            printf("%s\n", status);
        }
        return err;
    }
}

// Service command handlers
static infrax_error_t polyx_service_cmd_handle_rinetd(PolyxServiceCmd* self, 
                                                     const polyx_config_t* config, 
                                                     int argc, char** argv) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Get service instance
    PolyxService* service = PolyxServiceClass.get_service(self->service, POLYX_SERVICE_RINETD);
    if (!service) {
        fprintf(stderr, "Rinetd service not available\n");
        return INFRAX_ERROR_NOT_FOUND;
    }

    // Get config file path
    char config_file[POLYX_CMD_MAX_VALUE] = {0};
    PolyxCmdlineClass.get_option(self->cmdline, "--config", config_file, sizeof(config_file));

    return handle_service_command(self, service, config, config_file[0] ? config_file : NULL);
}

static infrax_error_t polyx_service_cmd_handle_sqlite(PolyxServiceCmd* self, 
                                                     const polyx_config_t* config, 
                                                     int argc, char** argv) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Get service instance
    PolyxService* service = PolyxServiceClass.get_service(self->service, POLYX_SERVICE_SQLITE);
    if (!service) {
        fprintf(stderr, "SQLite service not available\n");
        return INFRAX_ERROR_NOT_FOUND;
    }

    // Get config file path
    char config_file[POLYX_CMD_MAX_VALUE] = {0};
    PolyxCmdlineClass.get_option(self->cmdline, "--config", config_file, sizeof(config_file));

    return handle_service_command(self, service, config, config_file[0] ? config_file : NULL);
}

static infrax_error_t polyx_service_cmd_handle_memkv(PolyxServiceCmd* self, 
                                                    const polyx_config_t* config, 
                                                    int argc, char** argv) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Get service instance
    PolyxService* service = PolyxServiceClass.get_service(self->service, POLYX_SERVICE_MEMKV);
    if (!service) {
        fprintf(stderr, "MemKV service not available\n");
        return INFRAX_ERROR_NOT_FOUND;
    }

    // Get config file path
    char config_file[POLYX_CMD_MAX_VALUE] = {0};
    PolyxCmdlineClass.get_option(self->cmdline, "--config", config_file, sizeof(config_file));

    return handle_service_command(self, service, config, config_file[0] ? config_file : NULL);
}

// Service command registration
static infrax_error_t polyx_service_cmd_register_all(PolyxServiceCmd* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    static const polyx_cmd_t commands[] = {
        {
            .name = "rinetd",
            .desc = "Manage rinetd service",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = polyx_service_cmd_handle_rinetd
        },
        {
            .name = "sqlite",
            .desc = "Manage sqlite service",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = polyx_service_cmd_handle_sqlite
        },
        {
            .name = "memkv",
            .desc = "Manage memkv service",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = polyx_service_cmd_handle_memkv
        }
    };

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        infrax_error_t err = PolyxCmdlineClass.register_cmd(self->cmdline, &commands[i]);
        if (err != INFRAX_OK) {
            return err;
        }
    }

    return INFRAX_OK;
}

// Initialize memory
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
const PolyxServiceCmdClassType PolyxServiceCmdClass = {
    .new = polyx_service_cmd_new,
    .free = polyx_service_cmd_free,
    .handle_rinetd = polyx_service_cmd_handle_rinetd,
    .handle_sqlite = polyx_service_cmd_handle_sqlite,
    .handle_memkv = polyx_service_cmd_handle_memkv,
    .register_all = polyx_service_cmd_register_all
}; 