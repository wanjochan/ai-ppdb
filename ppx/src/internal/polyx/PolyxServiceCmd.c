#include "PolyxServiceCmd.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxCore.h"

// Command definitions
static const polyx_cmd_option_t service_options[] = {
    {"start", "Start the service", INFRAX_FALSE},
    {"stop", "Stop the service", INFRAX_FALSE},
    {"reload", "Reload the service", INFRAX_FALSE},
    {"status", "Show service status", INFRAX_FALSE},
    {"config", "Configuration file path", INFRAX_TRUE}
};

// Global instances
static InfraxMemory* g_memory = NULL;
static InfraxCore* g_core = NULL;

// Forward declarations
static InfraxBool init_memory(void);
static InfraxError handle_service_command(PolyxServiceCmd* self, PolyxService* service,
                                      const char* command, const char* config_file);

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
    g_core->memset(g_core, self, 0, sizeof(PolyxServiceCmd));
    self->self = self;
    self->klass = &PolyxServiceCmdClass;

    return self;
}

// Destructor
static void polyx_service_cmd_free(PolyxServiceCmd* self) {
    if (!self) return;
    g_memory->dealloc(g_memory, self);
}

// Service command handlers
static InfraxError handle_service_command(PolyxServiceCmd* self, PolyxService* service,
                                      const char* command, const char* config_file) {
    if (!self || !service || !command) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    if (g_core->strcmp(g_core, command, "start") == 0) {
        if (config_file) {
            polyx_config_t config;
            InfraxError err = polyx_config_parse_file(config_file, &config);
            if (!INFRAX_ERROR_IS_OK(err)) {
                g_core->printf(g_core, "Failed to parse config file: %s\n", config_file);
                return err;
            }

            // Register service with configuration
            if (config.service_count > 0) {
                err = PolyxServiceClass.register_service(self->service, &config.services[0]);
                if (!INFRAX_ERROR_IS_OK(err)) {
                    return err;
                }
            }
        }
        return service->start(service);
    }
    else if (g_core->strcmp(g_core, command, "stop") == 0) {
        return service->stop(service);
    }
    else if (g_core->strcmp(g_core, command, "reload") == 0) {
        return service->reload(service);
    }
    else if (g_core->strcmp(g_core, command, "status") == 0) {
        char status[1024];
        InfraxError err = service->get_status(service, status, sizeof(status));
        if (INFRAX_ERROR_IS_OK(err)) {
            g_core->printf(g_core, "%s\n", status);
        }
        return err;
    }
    else {
        char status[1024];
        InfraxError err = service->get_status(service, status, sizeof(status));
        if (INFRAX_ERROR_IS_OK(err)) {
            g_core->printf(g_core, "%s\n", status);
        }
        return err;
    }
}

static InfraxError polyx_service_cmd_handle_rinetd(PolyxServiceCmd* self,
                                                const polyx_config_t* config,
                                                InfraxI32 argc,
                                                char** argv) {
    if (!self || !config || argc < 2) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxService* service = PolyxServiceClass.get_service(self->service, POLYX_SERVICE_RINETD);
    if (!service) {
        g_core->printf(g_core, "Rinetd service not available\n");
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Service not found");
    }

    return handle_service_command(self, service, argv[1], argc > 2 ? argv[2] : NULL);
}

static InfraxError polyx_service_cmd_handle_sqlite(PolyxServiceCmd* self,
                                                const polyx_config_t* config,
                                                InfraxI32 argc,
                                                char** argv) {
    if (!self || !config || argc < 2) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxService* service = PolyxServiceClass.get_service(self->service, POLYX_SERVICE_SQLITE);
    if (!service) {
        g_core->printf(g_core, "SQLite service not available\n");
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Service not found");
    }

    return handle_service_command(self, service, argv[1], argc > 2 ? argv[2] : NULL);
}

static InfraxError polyx_service_cmd_handle_memkv(PolyxServiceCmd* self,
                                               const polyx_config_t* config,
                                               InfraxI32 argc,
                                               char** argv) {
    if (!self || !config || argc < 2) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxService* service = PolyxServiceClass.get_service(self->service, POLYX_SERVICE_MEMKV);
    if (!service) {
        g_core->printf(g_core, "MemKV service not available\n");
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Service not found");
    }

    return handle_service_command(self, service, argv[1], argc > 2 ? argv[2] : NULL);
}

// Service command registration
static InfraxError polyx_service_cmd_register_all(PolyxServiceCmd* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Register service commands
    static const polyx_cmd_t commands[] = {
        {
            .name = "rinetd",
            .desc = "Rinetd service management",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = (InfraxError (*)(const polyx_config_t*, InfraxI32, char**))polyx_service_cmd_handle_rinetd
        },
        {
            .name = "sqlite",
            .desc = "SQLite service management",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = (InfraxError (*)(const polyx_config_t*, InfraxI32, char**))polyx_service_cmd_handle_sqlite
        },
        {
            .name = "memkv",
            .desc = "MemKV service management",
            .options = service_options,
            .option_count = sizeof(service_options) / sizeof(service_options[0]),
            .handler = (InfraxError (*)(const polyx_config_t*, InfraxI32, char**))polyx_service_cmd_handle_memkv
        }
    };

    for (InfraxSize i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        InfraxError err = PolyxCmdlineClass.register_cmd(self->cmdline, &commands[i]);
        if (!INFRAX_ERROR_IS_OK(err)) {
            return err;
        }
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Memory initialization
static InfraxBool init_memory(void) {
    if (g_memory) return INFRAX_TRUE;

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
const PolyxServiceCmdClassType PolyxServiceCmdClass = {
    .new = polyx_service_cmd_new,
    .free = polyx_service_cmd_free,
    .handle_rinetd = polyx_service_cmd_handle_rinetd,
    .handle_sqlite = polyx_service_cmd_handle_sqlite,
    .handle_memkv = polyx_service_cmd_handle_memkv,
    .register_all = polyx_service_cmd_register_all
}; 