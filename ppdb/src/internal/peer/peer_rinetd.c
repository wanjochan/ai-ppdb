#include "internal/peer/peer_rinetd.h"
#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

static rinetd_context_t g_context = {0};

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static void* forward_session_thread(void* arg);
static void* listener_thread(void* arg);
static infra_error_t create_listener(rinetd_rule_t* rule);
static infra_error_t stop_listener(int rule_index);
static infra_error_t create_forward_session(infra_socket_t client_sock, rinetd_rule_t* rule);
static void cleanup_forward_session(rinetd_session_t* session);
static infra_error_t init_session_list(void);
static void cleanup_session_list(void);
static infra_error_t add_session_to_list(rinetd_session_t* session);
static void remove_session_from_list(rinetd_session_t* session);

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

const poly_cmd_option_t rinetd_options[] = {
    {"config", "Config file path", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show rinetd service status", false},
};

const int rinetd_option_count = sizeof(rinetd_options) / sizeof(rinetd_options[0]);

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static void* forward_session_thread(void* arg) {
    rinetd_session_t* session = (rinetd_session_t*)arg;
    char buffer[RINETD_BUFFER_SIZE];
    infra_socket_t socks[2] = {session->client_sock, session->server_sock};
    
    while (session->active) {
        // Wait for data on either socket
        infra_error_t err = infra_mux_wait(g_context.mux, socks, 2, -1);
        if (err != INFRA_OK) {
            break;
        }

        // Forward data in both directions
        for (int i = 0; i < 2; i++) {
            infra_socket_t src = socks[i];
            infra_socket_t dst = socks[1-i];
            
            int bytes = infra_socket_recv(src, buffer, sizeof(buffer), 0);
            if (bytes <= 0) {
                session->active = false;
                break;
            }

            int sent = infra_socket_send(dst, buffer, bytes, 0);
            if (sent != bytes) {
                session->active = false;
                break;
            }

            // TODO: Update statistics
        }
    }

    cleanup_forward_session(session);
    return NULL;
}

static void* listener_thread(void* arg) {
    rinetd_rule_t* rule = (rinetd_rule_t*)arg;
    infra_socket_t listener = infra_socket_create(INFRA_SOCKET_TCP);
    
    if (listener == 0) {
        INFRA_LOG_ERROR("Failed to create listener socket");
        return NULL;
    }

    // Bind to source address and port
    infra_error_t err = infra_socket_bind(listener, rule->src_addr, rule->src_port);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to bind to %s:%d", rule->src_addr, rule->src_port);
        infra_socket_close(listener);
        return NULL;
    }

    // Start listening
    err = infra_socket_listen(listener, 5);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to listen on %s:%d", rule->src_addr, rule->src_port);
        infra_socket_close(listener);
        return NULL;
    }

    INFRA_LOG_INFO("Listening on %s:%d", rule->src_addr, rule->src_port);

    while (g_context.running && rule->enabled) {
        // Accept new connection
        infra_socket_t client = infra_socket_accept(listener);
        if (client == 0) {
            continue;
        }

        // Create forward session
        err = create_forward_session(client, rule);
        if (err != INFRA_OK) {
            infra_socket_close(client);
        }
    }

    infra_socket_close(listener);
    return NULL;
}

static infra_error_t create_listener(rinetd_rule_t* rule) {
    infra_thread_t* thread = infra_thread_create(listener_thread, rule);
    if (thread == NULL) {
        INFRA_LOG_ERROR("Failed to create listener thread");
        return INFRA_ERROR_NO_MEMORY;
    }

    // Store thread handle
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (g_context.listener_threads[i] == NULL) {
            g_context.listener_threads[i] = thread;
            break;
        }
    }

    return INFRA_OK;
}

static infra_error_t stop_listener(int rule_index) {
    if (g_context.listener_threads[rule_index] != NULL) {
        infra_thread_join(g_context.listener_threads[rule_index]);
        g_context.listener_threads[rule_index] = NULL;
    }
    return INFRA_OK;
}

static infra_error_t create_forward_session(infra_socket_t client_sock, rinetd_rule_t* rule) {
    // Connect to destination
    infra_socket_t server_sock = infra_socket_create(INFRA_SOCKET_TCP);
    if (server_sock == 0) {
        INFRA_LOG_ERROR("Failed to create server socket");
        return INFRA_ERROR_NO_MEMORY;
    }

    infra_error_t err = infra_socket_connect(server_sock, rule->dst_addr, rule->dst_port);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to connect to %s:%d", rule->dst_addr, rule->dst_port);
        infra_socket_close(server_sock);
        return err;
    }

    // Create session
    rinetd_session_t session = {0};
    session.client_sock = client_sock;
    session.server_sock = server_sock;
    session.rule = rule;
    session.active = true;

    // Create forwarding thread
    session.thread = infra_thread_create(forward_session_thread, &session);
    if (session.thread == NULL) {
        INFRA_LOG_ERROR("Failed to create forwarding thread");
        infra_socket_close(server_sock);
        return INFRA_ERROR_NO_MEMORY;
    }

    // Add to active sessions list
    err = add_session_to_list(&session);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to add session to list");
        infra_socket_close(server_sock);
        infra_thread_join(session.thread);
        return err;
    }

    return INFRA_OK;
}

static void cleanup_forward_session(rinetd_session_t* session) {
    if (session == NULL) {
        return;
    }

    session->active = false;
    
    if (session->client_sock != 0) {
        infra_socket_close(session->client_sock);
        session->client_sock = 0;
    }
    
    if (session->server_sock != 0) {
        infra_socket_close(session->server_sock);
        session->server_sock = 0;
    }

    if (session->thread != NULL) {
        infra_thread_join(session->thread);
        session->thread = NULL;
    }

    // Remove from active sessions list
    remove_session_from_list(session);
}

static infra_error_t init_session_list(void) {
    // Allocate initial session array
    g_context.active_sessions = (rinetd_session_t*)infra_malloc(
        RINETD_MAX_RULES * sizeof(rinetd_session_t));
    if (g_context.active_sessions == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    memset(g_context.active_sessions, 0, 
        RINETD_MAX_RULES * sizeof(rinetd_session_t));
    g_context.session_count = 0;
    return INFRA_OK;
}

static void cleanup_session_list(void) {
    if (g_context.active_sessions != NULL) {
        // Cleanup all active sessions
        for (int i = 0; i < g_context.session_count; i++) {
            if (g_context.active_sessions[i].active) {
                cleanup_forward_session(&g_context.active_sessions[i]);
            }
        }
        
        infra_free(g_context.active_sessions);
        g_context.active_sessions = NULL;
        g_context.session_count = 0;
    }
}

static infra_error_t add_session_to_list(rinetd_session_t* session) {
    if (g_context.session_count >= RINETD_MAX_RULES) {
        return INFRA_ERROR_NO_MEMORY;
    }

    memcpy(&g_context.active_sessions[g_context.session_count], 
        session, sizeof(rinetd_session_t));
    g_context.session_count++;
    return INFRA_OK;
}

static void remove_session_from_list(rinetd_session_t* session) {
    for (int i = 0; i < g_context.session_count; i++) {
        if (&g_context.active_sessions[i] == session) {
            // Move last session to this slot if not the last one
            if (i < g_context.session_count - 1) {
                memcpy(&g_context.active_sessions[i],
                    &g_context.active_sessions[g_context.session_count - 1],
                    sizeof(rinetd_session_t));
            }
            g_context.session_count--;
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// Core Functions Implementation
//-----------------------------------------------------------------------------

infra_error_t rinetd_init(void) {
    if (g_context.mutex != NULL) {
        return INFRA_ERROR_ALREADY_INITIALIZED;
    }

    // Initialize mutex for thread synchronization
    g_context.mutex = infra_mutex_create();
    if (g_context.mutex == NULL) {
        INFRA_LOG_ERROR("Failed to create mutex");
        return INFRA_ERROR_NO_MEMORY;
    }

    // Initialize event multiplexer
    g_context.mux = infra_mux_create();
    if (g_context.mux == NULL) {
        INFRA_LOG_ERROR("Failed to create event multiplexer");
        infra_mutex_destroy(g_context.mutex);
        g_context.mutex = NULL;
        return INFRA_ERROR_NO_MEMORY;
    }

    // Initialize session list
    infra_error_t err = init_session_list();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize session list");
        infra_mux_destroy(g_context.mux);
        infra_mutex_destroy(g_context.mutex);
        g_context.mutex = NULL;
        g_context.mux = NULL;
        return err;
    }

    memset(g_context.rules, 0, sizeof(g_context.rules));
    g_context.rule_count = 0;
    g_context.running = false;

    return INFRA_OK;
}

infra_error_t rinetd_cleanup(void) {
    if (g_context.running) {
        INFRA_LOG_ERROR("Service is still running");
        return INFRA_ERROR_BUSY;
    }

    cleanup_session_list();

    if (g_context.mux != NULL) {
        infra_mux_destroy(g_context.mux);
        g_context.mux = NULL;
    }

    if (g_context.mutex != NULL) {
        infra_mutex_destroy(g_context.mutex);
        g_context.mutex = NULL;
    }

    memset(&g_context, 0, sizeof(g_context));
    return INFRA_OK;
}

infra_error_t rinetd_load_config(const char* path) {
    if (path == NULL) {
        INFRA_LOG_ERROR("Invalid config file path");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_context.running) {
        INFRA_LOG_ERROR("Cannot load config while service is running");
        return INFRA_ERROR_BUSY;
    }

    // Open config file
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // Clear existing rules
    memset(g_context.rules, 0, sizeof(g_context.rules));
    g_context.rule_count = 0;

    // Read rules from file
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        // Parse rule: src_addr src_port dst_addr dst_port
        rinetd_rule_t rule = {0};
        if (sscanf(line, "%s %d %s %d",
            rule.src_addr, &rule.src_port,
            rule.dst_addr, &rule.dst_port) != 4) {
            INFRA_LOG_ERROR("Invalid rule format: %s", line);
            continue;
        }

        // Add rule
        rule.enabled = true;
        infra_error_t err = rinetd_add_rule(&rule);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to add rule: %s", line);
        }
    }

    fclose(fp);
    strncpy(g_context.config_path, path, INFRA_MAX_PATH_LEN - 1);
    return INFRA_OK;
}

infra_error_t rinetd_save_config(const char* path) {
    if (path == NULL) {
        INFRA_LOG_ERROR("Invalid config file path");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Open config file
    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // Write header
    fprintf(fp, "# rinetd configuration file\n");
    fprintf(fp, "# format: src_addr src_port dst_addr dst_port\n\n");

    // Write rules
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.rules[i].enabled) {
            fprintf(fp, "%s %d %s %d\n",
                g_context.rules[i].src_addr,
                g_context.rules[i].src_port,
                g_context.rules[i].dst_addr,
                g_context.rules[i].dst_port);
        }
    }

    fclose(fp);
    return INFRA_OK;
}

infra_error_t rinetd_add_rule(const rinetd_rule_t* rule) {
    if (rule == NULL) {
        INFRA_LOG_ERROR("Invalid rule pointer");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_context.rule_count >= RINETD_MAX_RULES) {
        INFRA_LOG_ERROR("Too many rules");
        return INFRA_ERROR_NO_MEMORY;
    }

    // Check for duplicate rules
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.rules[i].enabled &&
            g_context.rules[i].src_port == rule->src_port &&
            strcmp(g_context.rules[i].src_addr, rule->src_addr) == 0) {
            INFRA_LOG_ERROR("Rule already exists for %s:%d",
                rule->src_addr, rule->src_port);
            return INFRA_ERROR_EXISTS;
        }
    }

    // Add new rule
    memcpy(&g_context.rules[g_context.rule_count], rule, sizeof(rinetd_rule_t));
    g_context.rule_count++;

    return INFRA_OK;
}

infra_error_t rinetd_remove_rule(int index) {
    if (index < 0 || index >= g_context.rule_count) {
        INFRA_LOG_ERROR("Invalid rule index");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_context.running) {
        INFRA_LOG_ERROR("Cannot remove rule while service is running");
        return INFRA_ERROR_BUSY;
    }

    // Disable the rule
    g_context.rules[index].enabled = false;

    // Compact rules array if this was the last rule
    if (index == g_context.rule_count - 1) {
        g_context.rule_count--;
    }

    return INFRA_OK;
}

bool rinetd_is_running(void) {
    return g_context.running;
}

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static infra_error_t parse_config_file(const char* path) {
    if (path == NULL) {
        INFRA_LOG_ERROR("Invalid config file path");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_context.running) {
        INFRA_LOG_ERROR("Cannot change configuration while service is running");
        return INFRA_ERROR_BUSY;
    }

    // Actually load the config
    return rinetd_load_config(path);
}

static infra_error_t start_service(void) {
    infra_error_t err;

    if (g_context.mutex == NULL) {
        err = rinetd_init();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to initialize service");
            return err;
        }
    }

    infra_mutex_lock(g_context.mutex);
    
    if (g_context.running) {
        infra_mutex_unlock(g_context.mutex);
        INFRA_LOG_ERROR("Service is already running");
        return INFRA_ERROR_ALREADY_STARTED;
    }

    // Check if we have any rules
    if (g_context.rule_count == 0) {
        infra_mutex_unlock(g_context.mutex);
        INFRA_LOG_ERROR("No forwarding rules configured");
        return INFRA_ERROR_INVALID_STATE;
    }

    // Start listener threads for each enabled rule
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.rules[i].enabled) {
            err = create_listener(&g_context.rules[i]);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to create listener for rule %d", i);
                // Continue with other rules
            }
        }
    }

    g_context.running = true;
    infra_mutex_unlock(g_context.mutex);

    infra_printf("Starting rinetd service...\n");
    return INFRA_OK;
}

static infra_error_t stop_service(void) {
    if (g_context.mutex == NULL) {
        INFRA_LOG_ERROR("Service is not initialized");
        return INFRA_ERROR_NOT_INITIALIZED;
    }

    infra_mutex_lock(g_context.mutex);
    
    if (!g_context.running) {
        infra_mutex_unlock(g_context.mutex);
        INFRA_LOG_ERROR("Service is not running");
        return INFRA_ERROR_NOT_STARTED;
    }

    // Stop all listeners first
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        stop_listener(i);
    }

    // Wait for all active sessions to complete
    infra_printf("Waiting for active sessions to complete...\n");
    while (g_context.session_count > 0) {
        rinetd_session_t* session = &g_context.active_sessions[0];
        cleanup_forward_session(session);
    }

    g_context.running = false;
    infra_mutex_unlock(g_context.mutex);

    infra_printf("Stopping rinetd service...\n");
    return INFRA_OK;
}

static infra_error_t show_status(void) {
    infra_printf("Checking rinetd service status...\n");
    
    if (g_context.mutex == NULL) {
        infra_printf("Service is not initialized\n");
        return INFRA_OK;
    }

    infra_mutex_lock(g_context.mutex);
    
    if (g_context.running) {
        infra_printf("Service is running\n");
        if (g_context.rule_count > 0) {
            infra_printf("Active forwarding rules:\n");
            for (int i = 0; i < g_context.rule_count; i++) {
                if (g_context.rules[i].enabled) {
                    infra_printf("  %s:%d -> %s:%d\n",
                        g_context.rules[i].src_addr,
                        g_context.rules[i].src_port,
                        g_context.rules[i].dst_addr,
                        g_context.rules[i].dst_port);
                }
            }
        } else {
            infra_printf("No active forwarding rules\n");
        }
    } else {
        infra_printf("Service is not running\n");
    }

    infra_mutex_unlock(g_context.mutex);
    return INFRA_OK;
}

infra_error_t rinetd_cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        INFRA_LOG_ERROR("Invalid arguments");
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* cmd = argv[1];
    if (strcmp(cmd, "--start") == 0) {
        return start_service();
    } else if (strcmp(cmd, "--stop") == 0) {
        return stop_service();
    } else if (strcmp(cmd, "--status") == 0) {
        return show_status();
    } else if (strcmp(cmd, "--config") == 0) {
        if (argc < 3) {
            INFRA_LOG_ERROR("Missing config file path");
            return INFRA_ERROR_INVALID_PARAM;
        }
        return parse_config_file(argv[2]);
    }

    INFRA_LOG_ERROR("rinetd: unknown operation: %s", cmd);
    return INFRA_ERROR_INVALID_PARAM;
} 