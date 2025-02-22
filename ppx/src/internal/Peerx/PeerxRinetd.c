#include "PeerxRinetd.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxLog.h"
#include "internal/polyx/PolyxPoll.h"

#define MAX_RULES 64
#define MAX_CONNECTIONS 1024

// Rule statistics
typedef struct {
    InfraxU64 bytes_in;
    InfraxU64 bytes_out;
    InfraxU64 connections;
} peerx_rinetd_stats_t;

// Connection state
typedef struct {
    InfraxNet* client;
    InfraxNet* target;
    InfraxBool active;
    InfraxU64 bytes_in;
    InfraxU64 bytes_out;
} peerx_rinetd_conn_t;

// Private data structure
typedef struct {
    InfraxMemory* memory;
    InfraxCore* core;
    InfraxNet* net;
    peerx_rinetd_rule_t rules[MAX_RULES];
    InfraxSize rule_count;
    peerx_rinetd_stats_t stats[MAX_RULES];
    peerx_rinetd_conn_t connections[MAX_CONNECTIONS];
    InfraxSize conn_count;
    InfraxBool initialized;
    InfraxBool running;
    PolyxPoll* poll;
} PeerxRinetdPrivate;

// Global instances
static InfraxMemory* g_memory = NULL;
static InfraxCore* g_core = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxCoreClassType InfraxCoreClass;
extern InfraxNetClassType InfraxNetClass;

// Forward declarations
static InfraxBool init_memory(void);
static InfraxI32 find_rule(PeerxRinetdPrivate* private, const char* bind_host, InfraxU16 bind_port);
static void handle_connection(PeerxRinetd* self, InfraxNet* client, InfraxI32 rule_index);
static InfraxError forward_data(InfraxNet* client, InfraxNet* server);

// Service lifecycle functions
static InfraxError rinetd_init(PolyxService* service) {
    if (!service) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxRinetd* self = (PeerxRinetd*)service;
    PeerxRinetdPrivate* private = service->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Initialize rules
    g_core->memset(g_core, private->rules, 0, sizeof(private->rules));
    private->rule_count = 0;
    private->initialized = INFRAX_TRUE;

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError rinetd_cleanup(PolyxService* service) {
    if (!service) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxRinetd* self = (PeerxRinetd*)service;
    PeerxRinetdPrivate* private = service->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Close all connections
    for (InfraxSize i = 0; i < private->conn_count; i++) {
        if (private->connections[i].client) {
            InfraxNetClass.free(private->connections[i].client);
        }
        if (private->connections[i].target) {
            InfraxNetClass.free(private->connections[i].target);
        }
    }

    // Free resources
    if (private->net) {
        InfraxNetClass.free(private->net);
    }
    if (private->poll) {
        PolyxPollClass.free(private->poll);
    }

    private->memory->dealloc(private->memory, private);
    service->private_data = NULL;

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError rinetd_start(PolyxService* service) {
    if (!service) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxRinetd* self = (PeerxRinetd*)service;
    PeerxRinetdPrivate* private = service->private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Create poll instance
    private->poll = PolyxPollClass.new();
    if (!private->poll) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to create poll instance");
    }

    // Start listening on all enabled rules
    for (InfraxSize i = 0; i < private->rule_count; i++) {
        if (!private->rules[i].enabled) continue;

        InfraxNetAddr addr = {
            .port = private->rules[i].bind_port
        };
        g_core->strcpy(g_core, addr.ip, private->rules[i].bind_host);

        InfraxNet* listener = InfraxNetClass.new(NULL);
        if (!listener) {
            POLYX_SERVICE_ERROR(service, "Failed to create listener for %s:%d",
                addr.ip, addr.port);
            continue;
        }

        InfraxError err = InfraxNetClass.bind(listener, &addr);
        if (err.code != INFRAX_ERROR_OK) {
            POLYX_SERVICE_ERROR(service, "Failed to bind to %s:%d",
                addr.ip, addr.port);
            InfraxNetClass.free(listener);
            continue;
        }

        err = InfraxNetClass.listen(listener, 5);
        if (err.code != INFRAX_ERROR_OK) {
            POLYX_SERVICE_ERROR(service, "Failed to listen on %s:%d",
                addr.ip, addr.port);
            InfraxNetClass.free(listener);
            continue;
        }

        err = PolyxPollClass.add(private->poll, listener, POLYX_POLL_IN);
        if (err.code != INFRAX_ERROR_OK) {
            POLYX_SERVICE_ERROR(service, "Failed to add listener to poll");
            InfraxNetClass.free(listener);
            continue;
        }

        POLYX_SERVICE_INFO(service, "Listening on %s:%d", addr.ip, addr.port);
    }

    private->running = INFRAX_TRUE;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError rinetd_stop(PolyxService* service) {
    if (!service) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxRinetd* self = (PeerxRinetd*)service;
    PeerxRinetdPrivate* private = service->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    private->running = INFRAX_FALSE;

    // Close all connections
    for (InfraxSize i = 0; i < private->conn_count; i++) {
        if (private->connections[i].client) {
            InfraxNetClass.free(private->connections[i].client);
        }
        if (private->connections[i].target) {
            InfraxNetClass.free(private->connections[i].target);
        }
    }
    private->conn_count = 0;

    // Free poll instance
    if (private->poll) {
        PolyxPollClass.free(private->poll);
        private->poll = NULL;
    }

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError rinetd_reload(PolyxService* service) {
    if (!service) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    // Stop service
    InfraxError err = rinetd_stop(service);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Start service again
    return rinetd_start(service);
}

static InfraxError rinetd_get_status(PolyxService* service, char* status, InfraxSize size) {
    if (!service || !status || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetd* self = (PeerxRinetd*)service;
    PeerxRinetdPrivate* private = service->private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    g_core->snprintf(g_core, status, size, "Rinetd service: %s\nRules: %zu, Active connections: %zu",
        private->running ? "running" : "stopped",
        private->rule_count, private->conn_count);

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Service factory
static PolyxService* create_rinetd_service(void) {
    // Initialize memory if needed
    if (!init_memory()) {
        return NULL;
    }

    // Allocate instance
    PeerxRinetd* self = g_memory->alloc(g_memory, sizeof(PeerxRinetd));
    if (!self) {
        return NULL;
    }

    // Initialize instance
    g_core->memset(g_core, self, 0, sizeof(PeerxRinetd));
    
    // Initialize service interface
    PolyxService* service = &self->service;
    service->self = service;
    service->init = rinetd_init;
    service->cleanup = rinetd_cleanup;
    service->start = rinetd_start;
    service->stop = rinetd_stop;
    service->reload = rinetd_reload;
    service->get_status = rinetd_get_status;

    // Allocate private data
    PeerxRinetdPrivate* private = g_memory->alloc(g_memory, sizeof(PeerxRinetdPrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    g_core->memset(g_core, private, 0, sizeof(PeerxRinetdPrivate));
    private->memory = g_memory;
    private->core = g_core;
    private->net = InfraxNetClass.new(NULL);  // Use default config
    if (!private->net) {
        g_memory->dealloc(g_memory, private);
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    service->private_data = private;
    return service;
}

// Rule management
static InfraxError peerx_rinetd_add_rule(PeerxRinetd* self, const peerx_rinetd_rule_t* rule) {
    if (!self || !rule) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->service.private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Check if rule already exists
    if (find_rule(private, rule->bind_host, rule->bind_port) >= 0) {
        return make_error(INFRAX_ERROR_FILE_EXISTS, "Rule already exists");
    }

    // Check if we have space
    if (private->rule_count >= MAX_RULES) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Maximum number of rules reached");
    }

    // Add rule
    g_core->memcpy(g_core, &private->rules[private->rule_count], rule, sizeof(peerx_rinetd_rule_t));
    private->rule_count++;

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_rinetd_remove_rule(PeerxRinetd* self, const char* bind_host, InfraxU16 bind_port) {
    if (!self || !bind_host) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->service.private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Find rule
    InfraxI32 index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Rule not found");
    }

    // Remove rule by shifting remaining rules
    if (index < private->rule_count - 1) {
        g_core->memmove(g_core, &private->rules[index], &private->rules[index + 1],
                (private->rule_count - index - 1) * sizeof(peerx_rinetd_rule_t));
    }

    private->rule_count--;
    g_core->memset(g_core, &private->rules[private->rule_count], 0, sizeof(peerx_rinetd_rule_t));

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_rinetd_enable_rule(PeerxRinetd* self, const char* bind_host, InfraxU16 bind_port) {
    if (!self || !bind_host) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->service.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    InfraxI32 index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        POLYX_SERVICE_ERROR(&self->service, "Rule not found for %s:%d", bind_host, bind_port);
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Rule not found");
    }

    private->rules[index].enabled = INFRAX_TRUE;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_rinetd_disable_rule(PeerxRinetd* self, const char* bind_host, InfraxU16 bind_port) {
    if (!self || !bind_host) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->service.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    InfraxI32 index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        POLYX_SERVICE_ERROR(&self->service, "Rule not found for %s:%d", bind_host, bind_port);
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Rule not found");
    }

    private->rules[index].enabled = INFRAX_FALSE;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_rinetd_get_rules(PeerxRinetd* self, peerx_rinetd_rule_t* rules, InfraxSize* count) {
    if (!self || !rules || !count) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->service.private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Copy rules
    InfraxSize to_copy = *count < private->rule_count ? *count : private->rule_count;
    g_core->memcpy(g_core, rules, private->rules, to_copy * sizeof(peerx_rinetd_rule_t));
    *count = to_copy;

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Statistics
static InfraxError peerx_rinetd_get_stats(PeerxRinetd* self, const char* bind_host, InfraxU16 bind_port,
                                         InfraxU64* bytes_in, InfraxU64* bytes_out,
                                         InfraxU64* connections) {
    if (!self || !bind_host || !bytes_in || !bytes_out || !connections) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->service.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    InfraxI32 index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        POLYX_SERVICE_ERROR(&self->service, "Rule not found for %s:%d", bind_host, bind_port);
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Rule not found");
    }

    *bytes_in = private->stats[index].bytes_in;
    *bytes_out = private->stats[index].bytes_out;
    *connections = private->stats[index].connections;

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Private helper functions
static InfraxBool init_memory(void) {
    if (g_memory && g_core) return INFRAX_TRUE;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = INFRAX_FALSE,
        .use_pool = INFRAX_TRUE,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    if (!g_memory) return INFRAX_FALSE;

    g_core = InfraxCoreClass.singleton();
    return g_core != NULL;
}

static InfraxI32 find_rule(PeerxRinetdPrivate* private, const char* bind_host, InfraxU16 bind_port) {
    if (!private || !bind_host) return -1;

    for (InfraxSize i = 0; i < private->rule_count; i++) {
        if (g_core->strcmp(g_core, private->rules[i].bind_host, bind_host) == 0 &&
            private->rules[i].bind_port == bind_port) {
            return (InfraxI32)i;
        }
    }

    return -1;
}

// Global class instance
const PeerxRinetdClassType PeerxRinetdClass = {
    .create_service = create_rinetd_service,
    .add_rule = peerx_rinetd_add_rule,
    .remove_rule = peerx_rinetd_remove_rule,
    .enable_rule = peerx_rinetd_enable_rule,
    .disable_rule = peerx_rinetd_disable_rule,
    .get_rules = peerx_rinetd_get_rules,
    .get_stats = peerx_rinetd_get_stats
}; 