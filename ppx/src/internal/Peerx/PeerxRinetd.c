#include "PeerxRinetd.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxMemory.h"
// #include <string.h>
// #include <stdio.h>

#define MAX_RULES 64
#define MAX_CONNECTIONS 1024

// Rule statistics
typedef struct {
    uint64_t bytes_in;
    uint64_t bytes_out;
    uint64_t connections;
} peerx_rinetd_stats_t;

// Connection state
typedef struct {
    InfraxNet* client;
    InfraxNet* target;
    bool active;
    uint64_t bytes_in;
    uint64_t bytes_out;
} peerx_rinetd_conn_t;

// Private data structure
typedef struct {
    InfraxMemory* memory;
    InfraxCore* core;
    InfraxNet* net;
    peerx_rinetd_rule_t rules[MAX_RULES];
    size_t rule_count;
    peerx_rinetd_stats_t stats[MAX_RULES];
    peerx_rinetd_conn_t connections[MAX_CONNECTIONS];
    size_t conn_count;
    bool initialized;
} PeerxRinetdPrivate;

// Global memory manager and core
static InfraxMemory* g_memory = NULL;
static InfraxCore* g_core = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxCoreClassType InfraxCoreClass;
extern InfraxNetClassType InfraxNetClass;

// Forward declarations of private functions
static bool init_memory(void);
static ssize_t find_rule(PeerxRinetdPrivate* private, const char* bind_host, int bind_port);
static void handle_connection(PeerxRinetd* self, InfraxNet* client, int rule_index);

// Constructor
static PeerxRinetd* peerx_rinetd_new(void) {
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
    
    // Initialize base service
    PeerxService* base = PeerxServiceClass.new();
    if (!base) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    g_core->memcpy(g_core, &self->base, base, sizeof(PeerxService));
    g_memory->dealloc(g_memory, base);

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

    self->base.private_data = private;

    return self;
}

// Destructor
static void peerx_rinetd_free(PeerxRinetd* self) {
    if (!self) return;

    // Stop service if running
    if (self->base.is_running) {
        self->base.klass->stop(&self->base);
    }

    // Free private data
    PeerxRinetdPrivate* private = self->base.private_data;
    if (private) {
        // Close all connections
        for (size_t i = 0; i < private->conn_count; i++) {
            if (private->connections[i].client) {
                InfraxNetClass.free(private->connections[i].client);
            }
            if (private->connections[i].target) {
                InfraxNetClass.free(private->connections[i].target);
            }
        }
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Rule management
static InfraxError peerx_rinetd_add_rule(PeerxRinetd* self, const peerx_rinetd_rule_t* rule) {
    if (!self || !rule) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->base.private_data;
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

static InfraxError peerx_rinetd_remove_rule(PeerxRinetd* self, const char* bind_host, int bind_port) {
    if (!self || !bind_host) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Find rule
    ssize_t index = find_rule(private, bind_host, bind_port);
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

static InfraxError peerx_rinetd_enable_rule(PeerxRinetd* self, const char* bind_host, int bind_port) {
    if (!self || !bind_host) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    int index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        PEERX_SERVICE_ERROR(&self->base, "Rule not found for %s:%d", bind_host, bind_port);
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Rule not found");
    }

    private->rules[index].enabled = true;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_rinetd_disable_rule(PeerxRinetd* self, const char* bind_host, int bind_port) {
    if (!self || !bind_host) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    int index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        PEERX_SERVICE_ERROR(&self->base, "Rule not found for %s:%d", bind_host, bind_port);
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Rule not found");
    }

    private->rules[index].enabled = false;
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_rinetd_get_rules(PeerxRinetd* self, peerx_rinetd_rule_t* rules, size_t* count) {
    if (!self || !rules || !count) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Copy rules
    size_t to_copy = *count < private->rule_count ? *count : private->rule_count;
    g_core->memcpy(g_core, rules, private->rules, to_copy * sizeof(peerx_rinetd_rule_t));
    *count = to_copy;

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Statistics
static InfraxError peerx_rinetd_get_stats(PeerxRinetd* self, const char* bind_host, int bind_port,
                                         uint64_t* bytes_in, uint64_t* bytes_out,
                                         uint64_t* connections) {
    if (!self || !bind_host || !bytes_in || !bytes_out || !connections) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    int index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        PEERX_SERVICE_ERROR(&self->base, "Rule not found for %s:%d", bind_host, bind_port);
        return make_error(INFRAX_ERROR_FILE_NOT_FOUND, "Rule not found");
    }

    *bytes_in = private->stats[index].bytes_in;
    *bytes_out = private->stats[index].bytes_out;
    *connections = private->stats[index].connections;

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Service lifecycle
static InfraxError peerx_rinetd_init(PeerxRinetd* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Initialize base service
    InfraxError err = self->base.klass->init(&self->base, config);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Initialize rules
    g_core->memset(g_core, private->rules, 0, sizeof(private->rules));
    private->rule_count = 0;
    private->initialized = true;

    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError peerx_rinetd_start(PeerxRinetd* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private || !private->initialized) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Service not initialized");
    }

    // Start base service
    return self->base.klass->start(&self->base);
}

static InfraxError peerx_rinetd_stop(PeerxRinetd* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    // Stop base service
    return self->base.klass->stop(&self->base);
}

static InfraxError peerx_rinetd_reload(PeerxRinetd* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameter");
    }

    // Reload base service
    return self->base.klass->reload(&self->base);
}

// Status and error handling
static InfraxError peerx_rinetd_get_status(PeerxRinetd* self, char* status, size_t size) {
    if (!self || !status || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return make_error(INFRAX_ERROR_INVALID_STATE, "Invalid state");
    }

    // Get base status
    InfraxError err = self->base.klass->get_status(&self->base, status, size);
    if (err.code != INFRAX_ERROR_OK) {
        return err;
    }

    // Append our status
    g_core->snprintf(g_core, status, size, "%s\nRules: %zu, Active connections: %zu",
        status, private->rule_count, 0);  // TODO: Track active connections

    return make_error(INFRAX_ERROR_OK, NULL);
}

// Private helper functions
static bool init_memory(void) {
    if (g_memory && g_core) return true;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    if (!g_memory) return false;

    g_core = InfraxCoreClass.singleton();
    return g_core != NULL;
}

static ssize_t find_rule(PeerxRinetdPrivate* private, const char* bind_host, int bind_port) {
    if (!private || !bind_host) return -1;

    for (size_t i = 0; i < private->rule_count; i++) {
        if (g_core->strcmp(g_core, private->rules[i].bind_host, bind_host) == 0 &&
            private->rules[i].bind_port == bind_port) {
            return i;
        }
    }

    return -1;
}

static void handle_connection(PeerxRinetd* self, InfraxNet* client, int rule_index) {
    if (!self || !client || rule_index < 0) return;

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private || (size_t)rule_index >= private->rule_count) return;

    // Find free connection slot
    int conn_index = -1;
    for (size_t i = 0; i < MAX_CONNECTIONS; i++) {
        if (!private->connections[i].active) {
            conn_index = (int)i;
            break;
        }
    }

    if (conn_index < 0) {
        PEERX_SERVICE_ERROR(&self->base, "Maximum number of connections reached");
        InfraxNetClass.free(client);
        return;
    }

    // Create target connection
    InfraxNetConfig config = {
        .is_udp = INFRAX_FALSE,
        .is_nonblocking = INFRAX_TRUE,
        .reuse_addr = INFRAX_TRUE,
        .send_timeout_ms = 5000,
        .recv_timeout_ms = 5000
    };
    
    InfraxNet* target = InfraxNetClass.new(&config);
    if (!target) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to create target socket");
        InfraxNetClass.free(client);
        return;
    }

    // Connect to target
    InfraxNetAddr addr;
    InfraxError err = infrax_net_addr_from_string(private->rules[rule_index].target_host,
                                                 private->rules[rule_index].target_port,
                                                 &addr);
    if (err.code != INFRAX_ERROR_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to create target address %s:%d",
                          private->rules[rule_index].target_host,
                          private->rules[rule_index].target_port);
        InfraxNetClass.free(target);
        InfraxNetClass.free(client);
        return;
    }

    err = InfraxNetClass.connect(target, &addr);
    if (err.code != INFRAX_ERROR_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to connect to target %s:%d",
                          private->rules[rule_index].target_host,
                          private->rules[rule_index].target_port);
        InfraxNetClass.free(target);
        InfraxNetClass.free(client);
        return;
    }

    // Setup connection
    private->connections[conn_index].client = client;
    private->connections[conn_index].target = target;
    private->connections[conn_index].active = true;
    private->connections[conn_index].bytes_in = 0;
    private->connections[conn_index].bytes_out = 0;
    private->conn_count++;
    private->stats[rule_index].connections++;

    // TODO: Setup data handlers using async framework
}

static void handle_client_data(PeerxRinetd* self, InfraxNet* client, 
                             const void* data, size_t size, int conn_index) {
    if (!self || !client || !data || conn_index < 0) return;

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private || (size_t)conn_index >= MAX_CONNECTIONS) return;

    peerx_rinetd_conn_t* conn = &private->connections[conn_index];
    if (!conn->active || !conn->target) return;

    // Forward data to target
    size_t sent;
    InfraxError err = InfraxNetClass.send(conn->target, data, size, &sent);
    if (err.code != INFRAX_ERROR_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to forward data to target");
        return;
    }

    // Update statistics
    conn->bytes_in += sent;
}

static void handle_target_data(PeerxRinetd* self, InfraxNet* target,
                             const void* data, size_t size, int conn_index) {
    if (!self || !target || !data || conn_index < 0) return;

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private || (size_t)conn_index >= MAX_CONNECTIONS) return;

    peerx_rinetd_conn_t* conn = &private->connections[conn_index];
    if (!conn->active || !conn->client) return;

    // Forward data to client
    size_t sent;
    InfraxError err = InfraxNetClass.send(conn->client, data, size, &sent);
    if (err.code != INFRAX_ERROR_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to forward data to client");
        return;
    }

    // Update statistics
    conn->bytes_out += sent;
}

// Adapter functions for PeerxService methods
static const char* peerx_rinetd_get_error_adapter(PeerxRinetd* self) {
    return PeerxServiceClass.get_error(&self->base);
}

static void peerx_rinetd_clear_error_adapter(PeerxRinetd* self) {
    PeerxServiceClass.clear_error(&self->base);
}

static InfraxError peerx_rinetd_validate_config_adapter(PeerxRinetd* self, const polyx_service_config_t* config) {
    return PeerxServiceClass.validate_config(&self->base, config);
}

static InfraxError peerx_rinetd_apply_config_adapter(PeerxRinetd* self, const polyx_service_config_t* config) {
    return PeerxServiceClass.apply_config(&self->base, config);
}

// Global class instance
const PeerxRinetdClassType PeerxRinetdClass = {
    .new = peerx_rinetd_new,
    .free = peerx_rinetd_free,
    .init = peerx_rinetd_init,
    .start = peerx_rinetd_start,
    .stop = peerx_rinetd_stop,
    .reload = peerx_rinetd_reload,
    .get_status = peerx_rinetd_get_status,
    .get_error = peerx_rinetd_get_error_adapter,
    .clear_error = peerx_rinetd_clear_error_adapter,
    .validate_config = peerx_rinetd_validate_config_adapter,
    .apply_config = peerx_rinetd_apply_config_adapter,
    .add_rule = peerx_rinetd_add_rule,
    .remove_rule = peerx_rinetd_remove_rule,
    .enable_rule = peerx_rinetd_enable_rule,
    .disable_rule = peerx_rinetd_disable_rule,
    .get_rules = peerx_rinetd_get_rules,
    .get_stats = peerx_rinetd_get_stats
}; 