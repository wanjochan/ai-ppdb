#include "PolyxService.h"
#include "internal/infrax/InfraxMemory.h"
#include <string.h>
#include <stdio.h>

#define MAX_SERVICES 16
#define MAX_STATUS_LENGTH 1024

// Private data structure
typedef struct {
    PolyxService* services[MAX_SERVICES];
    size_t count;
    InfraxMemory* memory;
} PolyxServicePrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;

// Forward declarations of private functions
static bool init_memory(void);

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
    memset(self, 0, sizeof(PolyxService));
    self->self = self;
    self->klass = &PolyxServiceClass;

    // Allocate private data
    PolyxServicePrivate* private = g_memory->alloc(g_memory, sizeof(PolyxServicePrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    memset(private, 0, sizeof(PolyxServicePrivate));
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
        for (size_t i = 0; i < private->count; i++) {
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
static infrax_error_t polyx_service_register(PolyxService* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PolyxServicePrivate* private = self->private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    if (private->count >= MAX_SERVICES) {
        return INFRAX_ERROR_NO_MEMORY;
    }

    // Check for duplicate
    for (size_t i = 0; i < private->count; i++) {
        if (private->services[i]->config.type == config->type) {
            return INFRAX_ERROR_EXISTS;
        }
    }

    // Create new service
    PolyxService* service = polyx_service_new();
    if (!service) {
        return INFRAX_ERROR_NO_MEMORY;
    }

    // Initialize service
    memcpy(&service->config, config, sizeof(polyx_service_config_t));
    service->state = POLYX_SERVICE_STATE_INIT;

    // Add to registry
    private->services[private->count++] = service;
    return INFRAX_OK;
}

static PolyxService* polyx_service_get(PolyxService* self, polyx_service_type_t type) {
    if (!self) return NULL;

    PolyxServicePrivate* private = self->private_data;
    if (!private) return NULL;

    for (size_t i = 0; i < private->count; i++) {
        if (private->services[i]->config.type == type) {
            return private->services[i];
        }
    }

    return NULL;
}

// Service lifecycle management
static infrax_error_t polyx_service_start_all(PolyxService* self) {
    if (!self) return INFRAX_ERROR_INVALID_PARAM;

    PolyxServicePrivate* private = self->private_data;
    if (!private) return INFRAX_ERROR_INVALID_STATE;

    infrax_error_t last_error = INFRAX_OK;

    for (size_t i = 0; i < private->count; i++) {
        PolyxService* service = private->services[i];
        if (!service || !service->start) continue;

        if (service->state != POLYX_SERVICE_STATE_RUNNING) {
            infrax_error_t err = service->start(service);
            if (err != INFRAX_OK) {
                last_error = err;
                service->state = POLYX_SERVICE_STATE_ERROR;
            } else {
                service->state = POLYX_SERVICE_STATE_RUNNING;
            }
        }
    }

    return last_error;
}

static infrax_error_t polyx_service_stop_all(PolyxService* self) {
    if (!self) return INFRAX_ERROR_INVALID_PARAM;

    PolyxServicePrivate* private = self->private_data;
    if (!private) return INFRAX_ERROR_INVALID_STATE;

    infrax_error_t last_error = INFRAX_OK;

    for (size_t i = 0; i < private->count; i++) {
        PolyxService* service = private->services[i];
        if (!service || !service->stop) continue;

        if (service->state == POLYX_SERVICE_STATE_RUNNING) {
            infrax_error_t err = service->stop(service);
            if (err != INFRAX_OK) {
                last_error = err;
                service->state = POLYX_SERVICE_STATE_ERROR;
            } else {
                service->state = POLYX_SERVICE_STATE_STOPPED;
            }
        }
    }

    return last_error;
}

static infrax_error_t polyx_service_reload_all(PolyxService* self) {
    if (!self) return INFRAX_ERROR_INVALID_PARAM;

    PolyxServicePrivate* private = self->private_data;
    if (!private) return INFRAX_ERROR_INVALID_STATE;

    infrax_error_t last_error = INFRAX_OK;

    for (size_t i = 0; i < private->count; i++) {
        PolyxService* service = private->services[i];
        if (!service || !service->reload) continue;

        infrax_error_t err = service->reload(service);
        if (err != INFRAX_OK) {
            last_error = err;
            service->state = POLYX_SERVICE_STATE_ERROR;
        }
    }

    return last_error;
}

// Service status
static infrax_error_t polyx_service_get_status(PolyxService* self, polyx_service_type_t type, 
                                             char* status, size_t size) {
    if (!self || !status || size == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PolyxService* service = polyx_service_get(self, type);
    if (!service) {
        return INFRAX_ERROR_NOT_FOUND;
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

    snprintf(status, size, "%s: %s", 
             polyx_config_get_service_type_name(service->config.type),
             state_str);

    return INFRAX_OK;
}

static infrax_error_t polyx_service_get_all_status(PolyxService* self, char* status, size_t size) {
    if (!self || !status || size == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PolyxServicePrivate* private = self->private_data;
    if (!private) return INFRAX_ERROR_INVALID_STATE;

    char* ptr = status;
    size_t remaining = size;

    for (size_t i = 0; i < private->count; i++) {
        PolyxService* service = private->services[i];
        if (!service) continue;

        char service_status[MAX_STATUS_LENGTH];
        infrax_error_t err = polyx_service_get_status(self, service->config.type, 
                                                    service_status, 
                                                    sizeof(service_status));
        if (err != INFRAX_OK) {
            continue;
        }

        size_t len = strlen(service_status);
        if (len + 2 > remaining) {
            break;
        }

        if (i > 0) {
            *ptr++ = '\n';
            remaining--;
        }

        strcpy(ptr, service_status);
        ptr += len;
        remaining -= len;
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