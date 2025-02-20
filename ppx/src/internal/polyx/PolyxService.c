#include "PolyxService.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxCore.h"

#define MAX_SERVICES 16
#define MAX_STATUS_LENGTH 1024

// Private data structure
typedef struct {
    PolyxService* services[MAX_SERVICES];
    InfraxSize count;
    InfraxMemory* memory;
} PolyxServicePrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
static InfraxCore* g_core = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxCoreClassType InfraxCoreClass;

// Forward declarations of private functions
static InfraxBool init_memory(void);

// Constructor
static PolyxService* polyx_service_new(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Allocate instance
    PolyxService* self = g_memory->alloc(g_memory, sizeof(PolyxService));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    g_core->memset(g_core, self, 0, sizeof(PolyxService));
    self->self = self;
    self->klass = &PolyxServiceClass;

    // Allocate private data
    PolyxServicePrivate* private = g_memory->alloc(g_memory, sizeof(PolyxServicePrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    g_core->memset(g_core, private, 0, sizeof(PolyxServicePrivate));
    private->memory = g_memory;

    self->private_data = private;
    return self;
}

// Destructor
static void polyx_service_free(PolyxService* self) {
    if (!self) return;

    // Stop all services
    PolyxServiceClass.stop_all(self);

    // Cleanup services
    PolyxServicePrivate* private = self->private_data;
    if (private) {
        for (InfraxSize i = 0; i < private->count; i++) {
            PolyxService* service = private->services[i];
            if (service && service->cleanup) {
                service->cleanup(service);
            }
            if (service) {
                private->memory->dealloc(private->memory, service);
            }
        }
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Service registration
static InfraxError polyx_service_register(PolyxService* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxServicePrivate* private = self->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid state");
    }

    if (private->count >= MAX_SERVICES) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Too many services");
    }

    // Check for duplicate
    for (InfraxSize i = 0; i < private->count; i++) {
        if (private->services[i]->config.type == config->type) {
            return make_error(INFRAX_ERROR_FILE_EXISTS, "Service already exists");
        }
    }

    // Create new service
    PolyxService* service = polyx_service_new();
    if (!service) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to create service");
    }

    // Initialize service
    g_core->memcpy(g_core, &service->config, config, sizeof(polyx_service_config_t));
    service->state = POLYX_SERVICE_STATE_INIT;

    // Add to registry
    private->services[private->count++] = service;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static PolyxService* polyx_service_get(PolyxService* self, polyx_service_type_t type) {
    if (!self) return NULL;

    PolyxServicePrivate* private = self->private_data;
    if (!private) return NULL;

    for (InfraxSize i = 0; i < private->count; i++) {
        if (private->services[i]->config.type == type) {
            return private->services[i];
        }
    }

    return NULL;
}

// Service lifecycle management
static InfraxError polyx_service_start_all(PolyxService* self) {
    if (!self) return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");

    PolyxServicePrivate* private = self->private_data;
    if (!private) return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid state");

    InfraxError last_error = make_error(INFRAX_ERROR_OK, NULL);

    for (InfraxSize i = 0; i < private->count; i++) {
        PolyxService* service = private->services[i];
        if (!service || !service->start) continue;

        if (service->state != POLYX_SERVICE_STATE_RUNNING) {
            InfraxError err = service->start(service);
            if (!INFRAX_ERROR_IS_OK(err)) {
                last_error = err;
                service->state = POLYX_SERVICE_STATE_ERROR;
            } else {
                service->state = POLYX_SERVICE_STATE_RUNNING;
            }
        }
    }

    return last_error;
}

static InfraxError polyx_service_stop_all(PolyxService* self) {
    if (!self) return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");

    PolyxServicePrivate* private = self->private_data;
    if (!private) return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid state");

    InfraxError last_error = make_error(INFRAX_ERROR_OK, NULL);

    for (InfraxSize i = 0; i < private->count; i++) {
        PolyxService* service = private->services[i];
        if (!service || !service->stop) continue;

        if (service->state == POLYX_SERVICE_STATE_RUNNING) {
            InfraxError err = service->stop(service);
            if (!INFRAX_ERROR_IS_OK(err)) {
                last_error = err;
                service->state = POLYX_SERVICE_STATE_ERROR;
            } else {
                service->state = POLYX_SERVICE_STATE_STOPPED;
            }
        }
    }

    return last_error;
}

static InfraxError polyx_service_reload_all(PolyxService* self) {
    if (!self) return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");

    PolyxServicePrivate* private = self->private_data;
    if (!private) return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid state");

    InfraxError last_error = make_error(INFRAX_ERROR_OK, NULL);

    for (InfraxSize i = 0; i < private->count; i++) {
        PolyxService* service = private->services[i];
        if (!service || !service->reload) continue;

        InfraxError err = service->reload(service);
        if (!INFRAX_ERROR_IS_OK(err)) {
            last_error = err;
            service->state = POLYX_SERVICE_STATE_ERROR;
        }
    }

    return last_error;
}

// Service status
static InfraxError polyx_service_get_status(PolyxService* self, polyx_service_type_t type, 
                                         char* status, InfraxSize size) {
    if (!self || !status || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxService* service = polyx_service_get(self, type);
    if (!service) {
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Service not found");
    }

    if (service->get_status) {
        return service->get_status(service, status, size);
    }

    // Default status format
    const char* state_str;
    switch (service->state) {
        case POLYX_SERVICE_STATE_INIT: state_str = "initializing"; break;
        case POLYX_SERVICE_STATE_READY: state_str = "ready"; break;
        case POLYX_SERVICE_STATE_RUNNING: state_str = "running"; break;
        case POLYX_SERVICE_STATE_STOPPED: state_str = "stopped"; break;
        case POLYX_SERVICE_STATE_ERROR: state_str = "error"; break;
        default: state_str = "unknown";
    }

    g_core->snprintf(g_core, status, size, "%s: %s", 
             polyx_config_get_service_type_name(service->config.type),
             state_str);

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError polyx_service_get_all_status(PolyxService* self, char* status, InfraxSize size) {
    if (!self || !status || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PolyxServicePrivate* private = self->private_data;
    if (!private) return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid state");

    char* ptr = status;
    InfraxSize remaining = size;

    for (InfraxSize i = 0; i < private->count; i++) {
        char service_status[MAX_STATUS_LENGTH];
        InfraxError err = polyx_service_get_status(self, private->services[i]->config.type,
                                                service_status, sizeof(service_status));
        if (!INFRAX_ERROR_IS_OK(err)) continue;

        InfraxSize len = g_core->strlen(g_core, service_status);
        if (len + 2 > remaining) { // +2 for newline and null terminator
            break;
        }

        if (i > 0) {
            *ptr++ = '\n';
            remaining--;
        }

        g_core->strcpy(g_core, ptr, service_status);
        ptr += len;
        remaining -= len;
    }

    *ptr = '\0';
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
const PolyxServiceClassType PolyxServiceClass = {
    .new = polyx_service_new,
    .free = polyx_service_free,
    .register_service = polyx_service_register,
    .get_service = polyx_service_get,
    .start_all = polyx_service_start_all,
    .stop_all = polyx_service_stop_all,
    .reload_all = polyx_service_reload_all,
    .get_status = polyx_service_get_status,
    .get_all_status = polyx_service_get_all_status
}; 