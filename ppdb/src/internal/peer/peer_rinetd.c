#include "internal/peer/peer_rinetd.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_mux.h"

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
    infra_socket_t src = session->client_sock;
    infra_socket_t dst = session->server_sock;
    char buffer[RINETD_BUFFER_SIZE];
    size_t bytes_received = 0;
    size_t bytes_sent = 0;

    while (session->active) {
        // Forward data from source to destination
        if (src) {
            infra_error_t err = infra_net_recv(src, buffer, sizeof(buffer), &bytes_received);
            if (err != INFRA_OK || bytes_received == 0) {
                break;
            }

            if (dst) {
                err = infra_net_send(dst, buffer, bytes_received, &bytes_sent);
                if (err != INFRA_OK || bytes_sent != bytes_received) {
                    break;
                }
            }
        }
    }

    return NULL;
}

static void* listener_thread(void* arg) {
    rinetd_rule_t* rule = (rinetd_rule_t*)arg;
    infra_socket_t listener = NULL;
    infra_config_t config = {0};
    
    // Create and bind listener socket
    infra_net_addr_t addr = {0};
    addr.host = rule->src_addr;
    addr.port = rule->src_port;
    infra_error_t err = infra_net_listen(&addr, &listener, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to start listening");
        return NULL;
    }

    // Accept loop
    while (g_context.running) {
        infra_socket_t client = NULL;
        infra_net_addr_t client_addr = {0};
        err = infra_net_accept(listener, &client, &client_addr);
        if (err != INFRA_OK) {
            continue;
        }

        // Create forward session
        rinetd_session_t* session = create_forward_session(client, rule);
        if (!session) {
            infra_net_close(client);
            continue;
        }
    }

    infra_net_close(listener);
    return NULL;
}

static infra_error_t create_listener(rinetd_rule_t* rule) {
    infra_thread_t* thread = NULL;
    infra_error_t err = infra_thread_create(&thread, listener_thread, rule);
    if (err != INFRA_OK) {
        return err;
    }

    // Store thread handle
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (!g_context.listener_threads[i]) {
            g_context.listener_threads[i] = thread;
            break;
        }
    }

    return INFRA_OK;
}

static infra_error_t stop_listener(int rule_index) {
    infra_thread_t* thread = NULL;

    infra_mutex_lock(g_context.mutex);
    thread = g_context.listener_threads[rule_index];
    g_context.listener_threads[rule_index] = NULL;
    infra_mutex_unlock(g_context.mutex);

    if (thread != NULL) {
        infra_thread_join(thread);
    }
    return INFRA_OK;
}

static rinetd_session_t* create_forward_session(infra_socket_t client_sock, rinetd_rule_t* rule) {
    if (!client_sock || !rule) {
        return NULL;
    }

    // Connect to destination
    infra_socket_t server_sock = NULL;
    infra_config_t config = {0};
    infra_net_addr_t addr = {0};
    addr.host = rule->dst_addr;
    addr.port = rule->dst_port;
    infra_error_t err = infra_net_connect(&addr, &server_sock, &config);
    if (err != INFRA_OK) {
        return NULL;
    }

    // Find free session slot
    rinetd_session_t* session = NULL;
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (!g_context.active_sessions[i].active) {
            session = &g_context.active_sessions[i];
            break;
        }
    }

    if (!session) {
        infra_net_close(server_sock);
        return NULL;
    }

    // Initialize session
    session->client_sock = client_sock;
    session->server_sock = server_sock;
    session->rule = rule;
    session->active = true;

    // Create forwarding thread
    err = infra_thread_create(&session->thread, forward_session_thread, session);
    if (err != INFRA_OK) {
        session->active = false;
        infra_net_close(server_sock);
        return NULL;
    }

    g_context.session_count++;
    return session;
}

static void cleanup_forward_session(rinetd_session_t* session) {
    if (session == NULL) {
        return;
    }

    infra_mutex_lock(g_context.mutex);
    session->active = false;
    infra_mutex_unlock(g_context.mutex);
    
    if (session->client_sock != 0) {
        infra_net_close(session->client_sock);
        session->client_sock = 0;
    }
    
    if (session->server_sock != 0) {
        infra_net_close(session->server_sock);
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
    // We don't need to allocate dynamically anymore since we're using a fixed array
    memset(g_context.active_sessions, 0, 
        RINETD_MAX_RULES * sizeof(rinetd_session_t));
    g_context.session_count = 0;
    return INFRA_OK;
}

static void cleanup_session_list(void) {
    // Cleanup all active sessions
    infra_mutex_lock(g_context.mutex);
    int count = g_context.session_count;
    infra_mutex_unlock(g_context.mutex);

    while (count > 0) {
        infra_mutex_lock(g_context.mutex);
        if (g_context.session_count > 0) {
            rinetd_session_t* session = &g_context.active_sessions[0];
            infra_mutex_unlock(g_context.mutex);
            cleanup_forward_session(session);
        } else {
            infra_mutex_unlock(g_context.mutex);
            break;
        }
        
        infra_mutex_lock(g_context.mutex);
        count = g_context.session_count;
        infra_mutex_unlock(g_context.mutex);
    }
    
    // Clear all sessions
    memset(g_context.active_sessions, 0, 
        RINETD_MAX_RULES * sizeof(rinetd_session_t));
    g_context.session_count = 0;
}

static infra_error_t add_session_to_list(rinetd_session_t* session) {
    infra_mutex_lock(g_context.mutex);
    if (g_context.session_count >= RINETD_MAX_RULES) {
        infra_mutex_unlock(g_context.mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    memcpy(&g_context.active_sessions[g_context.session_count], 
        session, sizeof(rinetd_session_t));
    g_context.session_count++;
    infra_mutex_unlock(g_context.mutex);
    return INFRA_OK;
}

static void remove_session_from_list(rinetd_session_t* session) {
    infra_mutex_lock(g_context.mutex);
    for (int i = 0; i < g_context.session_count; i++) {
        // Compare session by sockets instead of pointer
        if (g_context.active_sessions[i].client_sock == session->client_sock &&
            g_context.active_sessions[i].server_sock == session->server_sock) {
            // Move last session to this slot if not the last one
            if (i < g_context.session_count - 1) {
                memcpy(&g_context.active_sessions[i],
                    &g_context.active_sessions[g_context.session_count - 1],
                    sizeof(rinetd_session_t));
            }
            // Clear the last slot
            memset(&g_context.active_sessions[g_context.session_count - 1], 
                0, sizeof(rinetd_session_t));
            g_context.session_count--;
            break;
        }
    }
    infra_mutex_unlock(g_context.mutex);
}

//-----------------------------------------------------------------------------
// Core Functions Implementation
//-----------------------------------------------------------------------------

infra_error_t rinetd_init(void) {
    if (g_context.mutex) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    memset(&g_context, 0, sizeof(g_context));

    // Create mutex
    infra_error_t err = infra_mutex_create(&g_context.mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // Create multiplexer
    err = infra_mux_create(NULL, &g_context.mux);
    if (err != INFRA_OK) {
        infra_mutex_destroy(g_context.mutex);
        g_context.mutex = NULL;
        return err;
    }

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
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!g_context.mutex) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    strncpy(g_context.config_path, path, RINETD_MAX_PATH_LEN - 1);
    g_context.config_path[RINETD_MAX_PATH_LEN - 1] = '\0';

    // TODO: Parse config file and load rules
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

    infra_mutex_lock(g_context.mutex);
    if (g_context.running) {
        infra_mutex_unlock(g_context.mutex);
        INFRA_LOG_ERROR("Cannot change configuration while service is running");
        return INFRA_ERROR_BUSY;
    }
    infra_mutex_unlock(g_context.mutex);

    // Actually load the config
    return rinetd_load_config(path);
}

static infra_error_t start_service(void) {
    if (!g_context.mutex) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    if (g_context.running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Check if we have any rules
    if (g_context.rule_count == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Start listeners for each rule
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.rules[i].enabled) {
            infra_error_t err = create_listener(&g_context.rules[i]);
            if (err != INFRA_OK) {
                stop_service();
                return err;
            }
        }
    }

    g_context.running = true;
    return INFRA_OK;
}

static infra_error_t stop_service(void) {
    if (!g_context.mutex) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    if (!g_context.running) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    // Stop accepting new connections
    g_context.running = false;

    // Stop all listener threads
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (g_context.listener_threads[i]) {
            infra_thread_join(g_context.listener_threads[i]);
            g_context.listener_threads[i] = NULL;
        }
    }

    // Stop all active sessions
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (g_context.active_sessions[i].active) {
            g_context.active_sessions[i].active = false;
            if (g_context.active_sessions[i].thread) {
                infra_thread_join(g_context.active_sessions[i].thread);
                g_context.active_sessions[i].thread = NULL;
            }
            if (g_context.active_sessions[i].client_sock) {
                infra_net_close(g_context.active_sessions[i].client_sock);
                g_context.active_sessions[i].client_sock = NULL;
            }
            if (g_context.active_sessions[i].server_sock) {
                infra_net_close(g_context.active_sessions[i].server_sock);
                g_context.active_sessions[i].server_sock = NULL;
            }
        }
    }

    g_context.session_count = 0;
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
            infra_printf("Active sessions: %d\n", g_context.session_count);
        } else {
            infra_printf("No active forwarding rules\n");
        }
    } else {
        infra_printf("Service is not running\n");
        if (g_context.rule_count > 0) {
            infra_printf("Configured forwarding rules:\n");
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
            infra_printf("No forwarding rules configured\n");
        }
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