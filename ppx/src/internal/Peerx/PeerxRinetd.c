#include "PeerxRinetdInterface.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxSocket.h"
#include <string.h>
#include <stdio.h>

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
    InfraxSocket* client;
    InfraxSocket* target;
    bool active;
    uint64_t bytes_in;
    uint64_t bytes_out;
} peerx_rinetd_conn_t;

// Private data structure
typedef struct {
    InfraxMemory* memory;
    peerx_rinetd_rule_t rules[MAX_RULES];
    size_t rule_count;
    peerx_rinetd_stats_t stats[MAX_RULES];
    peerx_rinetd_conn_t connections[MAX_CONNECTIONS];
    size_t conn_count;
} PeerxRinetdPrivate;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;
extern InfraxSocketClassType InfraxSocketClass;

// Forward declarations of private functions
static bool init_memory(void);
static int find_rule(PeerxRinetdPrivate* private, const char* bind_host, int bind_port);
static void handle_connection(PeerxRinetd* self, InfraxSocket* client, int rule_index);

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
    memset(self, 0, sizeof(PeerxRinetd));
    
    // Initialize base service
    PeerxService* base = PeerxServiceClass.new();
    if (!base) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    memcpy(&self->base, base, sizeof(PeerxService));
    g_memory->dealloc(g_memory, base);

    // Allocate private data
    PeerxRinetdPrivate* private = g_memory->alloc(g_memory, sizeof(PeerxRinetdPrivate));
    if (!private) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }

    // Initialize private data
    memset(private, 0, sizeof(PeerxRinetdPrivate));
    private->memory = g_memory;

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
                InfraxSocketClass.free(private->connections[i].client);
            }
            if (private->connections[i].target) {
                InfraxSocketClass.free(private->connections[i].target);
            }
        }
        private->memory->dealloc(private->memory, private);
    }

    g_memory->dealloc(g_memory, self);
}

// Rules management
static infrax_error_t peerx_rinetd_add_rule(PeerxRinetd* self, const peerx_rinetd_rule_t* rule) {
    if (!self || !rule) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    if (private->rule_count >= MAX_RULES) {
        PEERX_SERVICE_ERROR(&self->base, "Maximum number of rules reached");
        return INFRAX_ERROR_NO_MEMORY;
    }

    // Check for duplicate
    if (find_rule(private, rule->bind_host, rule->bind_port) >= 0) {
        PEERX_SERVICE_ERROR(&self->base, "Rule already exists for %s:%d",
                          rule->bind_host, rule->bind_port);
        return INFRAX_ERROR_EXISTS;
    }

    // Add rule
    memcpy(&private->rules[private->rule_count], rule, sizeof(peerx_rinetd_rule_t));
    private->rule_count++;

    return INFRAX_OK;
}

static infrax_error_t peerx_rinetd_remove_rule(PeerxRinetd* self, const char* bind_host, int bind_port) {
    if (!self || !bind_host) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    int index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        PEERX_SERVICE_ERROR(&self->base, "Rule not found for %s:%d", bind_host, bind_port);
        return INFRAX_ERROR_NOT_FOUND;
    }

    // Remove rule
    if (index < private->rule_count - 1) {
        memmove(&private->rules[index], &private->rules[index + 1],
                (private->rule_count - index - 1) * sizeof(peerx_rinetd_rule_t));
        memmove(&private->stats[index], &private->stats[index + 1],
                (private->rule_count - index - 1) * sizeof(peerx_rinetd_stats_t));
    }
    private->rule_count--;

    return INFRAX_OK;
}

static infrax_error_t peerx_rinetd_enable_rule(PeerxRinetd* self, const char* bind_host, int bind_port) {
    if (!self || !bind_host) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    int index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        PEERX_SERVICE_ERROR(&self->base, "Rule not found for %s:%d", bind_host, bind_port);
        return INFRAX_ERROR_NOT_FOUND;
    }

    private->rules[index].enabled = true;
    return INFRAX_OK;
}

static infrax_error_t peerx_rinetd_disable_rule(PeerxRinetd* self, const char* bind_host, int bind_port) {
    if (!self || !bind_host) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    int index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        PEERX_SERVICE_ERROR(&self->base, "Rule not found for %s:%d", bind_host, bind_port);
        return INFRAX_ERROR_NOT_FOUND;
    }

    private->rules[index].enabled = false;
    return INFRAX_OK;
}

static infrax_error_t peerx_rinetd_get_rules(PeerxRinetd* self, peerx_rinetd_rule_t* rules, size_t* count) {
    if (!self || !rules || !count) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    size_t to_copy = *count < private->rule_count ? *count : private->rule_count;
    memcpy(rules, private->rules, to_copy * sizeof(peerx_rinetd_rule_t));
    *count = to_copy;

    return INFRAX_OK;
}

// Statistics
static infrax_error_t peerx_rinetd_get_stats(PeerxRinetd* self, const char* bind_host, int bind_port,
                                            uint64_t* bytes_in, uint64_t* bytes_out,
                                            uint64_t* connections) {
    if (!self || !bind_host || !bytes_in || !bytes_out || !connections) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    int index = find_rule(private, bind_host, bind_port);
    if (index < 0) {
        PEERX_SERVICE_ERROR(&self->base, "Rule not found for %s:%d", bind_host, bind_port);
        return INFRAX_ERROR_NOT_FOUND;
    }

    *bytes_in = private->stats[index].bytes_in;
    *bytes_out = private->stats[index].bytes_out;
    *connections = private->stats[index].connections;

    return INFRAX_OK;
}

// Service lifecycle
static infrax_error_t peerx_rinetd_init(PeerxRinetd* self, const polyx_service_config_t* config) {
    if (!self || !config) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Initialize base service
    infrax_error_t err = PeerxServiceClass.init(&self->base, config);
    if (err != INFRAX_OK) {
        return err;
    }

    // Add initial rule from config
    peerx_rinetd_rule_t rule = {
        .bind_port = config->listen_port,
        .target_port = config->target_port,
        .enabled = true
    };
    strncpy(rule.bind_host, config->listen_host, sizeof(rule.bind_host) - 1);
    strncpy(rule.target_host, config->target_host, sizeof(rule.target_host) - 1);

    return peerx_rinetd_add_rule(self, &rule);
}

static infrax_error_t peerx_rinetd_start(PeerxRinetd* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Start base service
    infrax_error_t err = PeerxServiceClass.start(&self->base);
    if (err != INFRAX_OK) {
        return err;
    }

    // Start listening on all enabled rules
    PeerxRinetdPrivate* private = self->base.private_data;
    for (size_t i = 0; i < private->rule_count; i++) {
        if (!private->rules[i].enabled) continue;

        // Create listener socket
        InfraxSocket* listener = InfraxSocketClass.new();
        if (!listener) {
            PEERX_SERVICE_ERROR(&self->base, "Failed to create listener socket");
            continue;
        }

        // Bind and listen
        err = InfraxSocketClass.bind(listener, private->rules[i].bind_host,
                                   private->rules[i].bind_port);
        if (err != INFRAX_OK) {
            PEERX_SERVICE_ERROR(&self->base, "Failed to bind to %s:%d",
                              private->rules[i].bind_host,
                              private->rules[i].bind_port);
            InfraxSocketClass.free(listener);
            continue;
        }

        err = InfraxSocketClass.listen(listener, 5);
        if (err != INFRAX_OK) {
            PEERX_SERVICE_ERROR(&self->base, "Failed to listen on %s:%d",
                              private->rules[i].bind_host,
                              private->rules[i].bind_port);
            InfraxSocketClass.free(listener);
            continue;
        }

        // Register accept callback
        InfraxSocketClass.on_accept(listener, (void*)handle_connection, self, (void*)(intptr_t)i);
    }

    return INFRAX_OK;
}

static infrax_error_t peerx_rinetd_stop(PeerxRinetd* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Stop base service
    return PeerxServiceClass.stop(&self->base);
}

static infrax_error_t peerx_rinetd_reload(PeerxRinetd* self) {
    if (!self) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Reload base service
    return PeerxServiceClass.reload(&self->base);
}

// Status and error handling
static infrax_error_t peerx_rinetd_get_status(PeerxRinetd* self, char* status, size_t size) {
    if (!self || !status || size == 0) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private) {
        return INFRAX_ERROR_INVALID_STATE;
    }

    // Get base status
    char base_status[512];
    infrax_error_t err = PeerxServiceClass.get_status(&self->base, base_status, sizeof(base_status));
    if (err != INFRAX_OK) {
        return err;
    }

    // Add rinetd specific status
    snprintf(status, size, "%s\nRules: %zu, Active connections: %zu",
             base_status, private->rule_count, private->conn_count);

    return INFRAX_OK;
}

// Private helper functions
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

static int find_rule(PeerxRinetdPrivate* private, const char* bind_host, int bind_port) {
    for (size_t i = 0; i < private->rule_count; i++) {
        if (strcmp(private->rules[i].bind_host, bind_host) == 0 &&
            private->rules[i].bind_port == bind_port) {
            return (int)i;
        }
    }
    return -1;
}

static void handle_connection(PeerxRinetd* self, InfraxSocket* client, int rule_index) {
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
        InfraxSocketClass.free(client);
        return;
    }

    // Create target connection
    InfraxSocket* target = InfraxSocketClass.new();
    if (!target) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to create target socket");
        InfraxSocketClass.free(client);
        return;
    }

    // Connect to target
    infrax_error_t err = InfraxSocketClass.connect(target,
                                                  private->rules[rule_index].target_host,
                                                  private->rules[rule_index].target_port);
    if (err != INFRAX_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to connect to target %s:%d",
                          private->rules[rule_index].target_host,
                          private->rules[rule_index].target_port);
        InfraxSocketClass.free(target);
        InfraxSocketClass.free(client);
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

    // Setup data handlers
    InfraxSocketClass.on_data(client, (void*)handle_client_data, self, (void*)(intptr_t)conn_index);
    InfraxSocketClass.on_data(target, (void*)handle_target_data, self, (void*)(intptr_t)conn_index);
}

static void handle_client_data(PeerxRinetd* self, InfraxSocket* client, 
                             const void* data, size_t size, int conn_index) {
    if (!self || !client || !data || conn_index < 0) return;

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private || (size_t)conn_index >= MAX_CONNECTIONS) return;

    peerx_rinetd_conn_t* conn = &private->connections[conn_index];
    if (!conn->active || !conn->target) return;

    // Forward data to target
    infrax_error_t err = InfraxSocketClass.write(conn->target, data, size);
    if (err != INFRAX_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to forward data to target");
        return;
    }

    // Update statistics
    conn->bytes_in += size;
}

static void handle_target_data(PeerxRinetd* self, InfraxSocket* target,
                             const void* data, size_t size, int conn_index) {
    if (!self || !target || !data || conn_index < 0) return;

    PeerxRinetdPrivate* private = self->base.private_data;
    if (!private || (size_t)conn_index >= MAX_CONNECTIONS) return;

    peerx_rinetd_conn_t* conn = &private->connections[conn_index];
    if (!conn->active || !conn->client) return;

    // Forward data to client
    infrax_error_t err = InfraxSocketClass.write(conn->client, data, size);
    if (err != INFRAX_OK) {
        PEERX_SERVICE_ERROR(&self->base, "Failed to forward data to client");
        return;
    }

    // Update statistics
    conn->bytes_out += size;
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
    .get_error = PeerxServiceClass.get_error,
    .clear_error = PeerxServiceClass.clear_error,
    .validate_config = PeerxServiceClass.validate_config,
    .apply_config = PeerxServiceClass.apply_config
}; 