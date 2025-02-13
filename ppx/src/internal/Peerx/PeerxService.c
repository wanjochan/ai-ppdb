#include "PeerxService.h"
#include "internal/infrax/InfraxMemory.h"
#include <string.h>
#include <stdio.h>

// Private data structure
typedef struct {
    InfraxMemory* memory;
} PeerxServicePrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;

// Forward declarations of private functions
static bool init_memory(void);

// Constructor
static PeerxService* peerx_service_new(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Allocate instance
    PeerxService* self = g_memory->alloc(g_memory, sizeof(PeerxService));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    memset(self, 0, sizeof(PeerxService));
    self->self = self;
    self->klass = &PeerxServiceClass;

    // Create async instance
    self->async = InfraxAsyncClass.new();
    if (!self->async) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Allocate private data
    PeerxServicePrivate* private = g_memory->alloc(g_memory, sizeof(PeerxServicePrivate));
    if (!private) {
        InfraxAsyncClass.free(self->async);
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    memset(private, 0, sizeof(PeerxServicePrivate));
    private->memory = g_memory;

    self->private_data = private;
    return self;
}

// Destructor
static void peerx_service_free(PeerxService* self) {
    if (!self) return;

    // Stop service if running
    if (self->is_running) {
        self->klass->stop(self);
    }

    // Free error message
    if (self->error_message) {
        free(self->error_message);
    }

    // Free async instance
    if (self->async) {
        InfraxAsyncClass.free(self->async);
    }

    // Free private data
    PeerxServicePrivate* private = self->private_data;
    if (private) {
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Service lifecycle
static infrax_error_t peerx_service_init(PeerxService* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Validate configuration
    infrax_error_t err = self->klass->validate_config(self, config);
    if (err != INFRAX_OK) {
        return err;
    }

    // Apply configuration
    err = self->klass->apply_config(self, config);
    if (err != INFRAX_OK) {
        return err;
    }

    self->is_initialized = true;
    return INFRAX_OK;
}

static infrax_error_t peerx_service_start(PeerxService* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    if (!self->is_initialized) {
        PEERX_SERVICE_ERROR(self, "Service not initialized");
        return INFRAX_ERROR_INVALID_STATE;
    }

    if (self->is_running) {
        PEERX_SERVICE_ERROR(self, "Service already running");
        return INFRAX_ERROR_INVALID_STATE;
    }

    self->is_running = true;
    return INFRAX_OK;
}

static infrax_error_t peerx_service_stop(PeerxService* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    if (!self->is_running) {
        PEERX_SERVICE_ERROR(self, "Service not running");
        return INFRAX_ERROR_INVALID_STATE;
    }

    self->is_running = false;
    return INFRAX_OK;
}

static infrax_error_t peerx_service_reload(PeerxService* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    if (!self->is_initialized) {
        PEERX_SERVICE_ERROR(self, "Service not initialized");
        return INFRAX_ERROR_INVALID_STATE;
    }

    return INFRAX_OK;
}

// Status and error handling
static infrax_error_t peerx_service_get_status(PeerxService* self, char* status, size_t size) {
    if (!self || !status || size == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    const char* state = self->is_running ? "running" : 
                       self->is_initialized ? "initialized" : "stopped";
    
    snprintf(status, size, "State: %s%s%s", 
             state,
             self->error_message ? ", Error: " : "",
             self->error_message ? self->error_message : "");

    return INFRAX_OK;
}

static const char* peerx_service_get_error(PeerxService* self) {
    return self ? self->error_message : NULL;
}

static void peerx_service_clear_error(PeerxService* self) {
    if (!self) return;
    
    if (self->error_message) {
        free(self->error_message);
        self->error_message = NULL;
    }
}

// Configuration
static infrax_error_t peerx_service_validate_config(PeerxService* self, 
                                                  const polyx_service_config_t* config) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    return INFRAX_OK;
}

static infrax_error_t peerx_service_apply_config(PeerxService* self, 
                                               const polyx_service_config_t* config) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
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
const PeerxServiceClassType PeerxServiceClass = {
    .new = peerx_service_new,
    .free = peerx_service_free,
    .init = peerx_service_init,
    .start = peerx_service_start,
    .stop = peerx_service_stop,
    .reload = peerx_service_reload,
    .get_status = peerx_service_get_status,
    .get_error = peerx_service_get_error,
    .clear_error = peerx_service_clear_error,
    .validate_config = peerx_service_validate_config,
    .apply_config = peerx_service_apply_config
}; 