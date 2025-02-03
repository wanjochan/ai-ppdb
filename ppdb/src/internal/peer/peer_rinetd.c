#include "internal/peer/peer_rinetd.h"
#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_log.h"
#include <poll.h>
#include <errno.h>
#include <string.h>

// Default configuration
static rinetd_config_t g_rinetd_default_config = {
    .rules = {
        .count = 0
    }
};

// Global service instance
peer_service_t g_rinetd_service = {
    .config = {
        .name = "rinetd",
        .user_data = NULL
    },
    .state = PEER_SERVICE_STATE_INIT,
    .init = rinetd_init,
    .cleanup = rinetd_cleanup,
    .start = rinetd_start,
    .stop = rinetd_stop,
    .cmd_handler = rinetd_cmd_handler,
};

// Global variables
static struct {
    bool running;
} g_rinetd_state = {0};

// Forward declarations
static void* handle_connection(void* args);
static void* handle_connection_thread(void* args);

// Find forward rule for address and port
static rinetd_rule_t* find_forward_rule(const char* addr, uint16_t port) {
    for (int i = 0; i < g_rinetd_default_config.rules.count; i++) {
        rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[i];
        if (strcmp(rule->src_addr, addr) == 0 && rule->src_port == port) {
            return rule;
        }
    }
    return NULL;
}

// Handle a client connection
static void* handle_connection(void* args) {
    rinetd_rule_t* rule = (rinetd_rule_t*)args;
    if (!rule || rule->listener < 0) {
        return NULL;
    }

    while (g_rinetd_state.running) {
        infra_socket_t client = -1;
        infra_net_addr_t client_addr;
        infra_error_t err = infra_net_accept(rule->listener, &client, &client_addr);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                // Non-blocking mode, no connection available
                usleep(10000);  // Sleep 10ms
                continue;
            }
            if (g_rinetd_state.running) {
                INFRA_LOG_ERROR("Failed to accept connection: %d", err);
            }
            break;
        }

        // Create a new session
        rinetd_session_t* session = malloc(sizeof(rinetd_session_t));
        if (!session) {
            INFRA_LOG_ERROR("Failed to allocate session memory");
            infra_net_close(client);
            continue;
        }

        session->client = client;
        memcpy(&session->rule, rule, sizeof(rinetd_rule_t));

        // Handle the connection in a new thread
        infra_thread_t thread;
        err = infra_thread_create(&thread, handle_connection_thread, session);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to create thread: %d", err);
            free(session);
            infra_net_close(client);
        }
    }

    return NULL;
}

static void* handle_connection_thread(void* args) {
    rinetd_session_t* session = (rinetd_session_t*)args;
    if (!session) {
        INFRA_LOG_ERROR("Invalid session");
        return NULL;
    }

    // Get client address
    infra_net_addr_t client_addr;
    infra_error_t err = infra_net_get_peer_addr(session->client, &client_addr);
    if (err == INFRA_OK) {
        INFRA_LOG_INFO("New client connection from %s:%d", client_addr.ip, client_addr.port);
    }

    INFRA_LOG_INFO("New connection, forwarding to %s:%d", 
        session->rule.dst_addr, session->rule.dst_port);

    // Connect to destination
    infra_socket_t server = -1;
    infra_net_addr_t dst_addr = {
        .port = session->rule.dst_port
    };
    strncpy(dst_addr.ip, session->rule.dst_addr, sizeof(dst_addr.ip) - 1);

    INFRA_LOG_INFO("Connecting to %s:%d", dst_addr.ip, dst_addr.port);
    err = infra_net_connect(&dst_addr, &server);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to connect to %s:%d: %d (errno=%d: %s)", 
            dst_addr.ip, dst_addr.port, err, errno, strerror(errno));
        infra_net_close(session->client);
        free(session);
        return NULL;
    }
    INFRA_LOG_INFO("Connected to %s:%d", dst_addr.ip, dst_addr.port);

    // Forward data
    char buffer[4096];
    size_t received = 0;
    size_t sent = 0;

    // Client -> Server
    INFRA_LOG_INFO("Waiting for data from client...");
    err = infra_net_recv(session->client, buffer, sizeof(buffer), &received);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to receive from client: %d (errno=%d: %s)", 
            err, errno, strerror(errno));
        goto cleanup;
    }
    INFRA_LOG_INFO("Received %zu bytes from client: %.100s%s", 
        received, buffer, received > 100 ? "..." : "");

    INFRA_LOG_INFO("Sending data to server...");
    err = infra_net_send(server, buffer, received, &sent);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send to server: %d (errno=%d: %s)", 
            err, errno, strerror(errno));
        goto cleanup;
    }
    INFRA_LOG_INFO("Sent %zu bytes to server", sent);

    // Server -> Client
    received = 0;
    sent = 0;
    INFRA_LOG_INFO("Waiting for response from server...");
    err = infra_net_recv(server, buffer, sizeof(buffer), &received);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to receive from server: %d (errno=%d: %s)", 
            err, errno, strerror(errno));
        goto cleanup;
    }
    INFRA_LOG_INFO("Received %zu bytes from server: %.100s%s", 
        received, buffer, received > 100 ? "..." : "");

    INFRA_LOG_INFO("Sending response to client...");
    err = infra_net_send(session->client, buffer, received, &sent);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send to client: %d (errno=%d: %s)", 
            err, errno, strerror(errno));
        goto cleanup;
    }
    INFRA_LOG_INFO("Sent %zu bytes to client", sent);

cleanup:
    INFRA_LOG_INFO("Closing connection");
    infra_net_close(server);
    infra_net_close(session->client);
    free(session);
    INFRA_LOG_INFO("Connection closed");
    return NULL;
}

// Initialize rinetd service
infra_error_t rinetd_init(void) {
    if (g_rinetd_service.state != PEER_SERVICE_STATE_INIT &&
        g_rinetd_service.state != PEER_SERVICE_STATE_STOPPED) {
        return INFRA_ERROR_INVALID_STATE;
    }

    // Get config file path from command line
    // Since we don't have direct access to command line arguments,
    // we'll use a default config file path for now
    const char* config_path = "ppdb/rinetd2.conf";
    if (config_path) {
        INFRA_LOG_INFO("Loading config from %s", config_path);
        infra_error_t err = rinetd_load_config(config_path);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to load config: %d", err);
            return err;
        }
    }

    g_rinetd_service.state = PEER_SERVICE_STATE_READY;
    return INFRA_OK;
}

// Start rinetd service
infra_error_t rinetd_start(void) {
    if (g_rinetd_service.state != PEER_SERVICE_STATE_READY) {
        return INFRA_ERROR_INVALID_STATE;
    }

    // Start all rules
    for (int i = 0; i < g_rinetd_default_config.rules.count; i++) {
        rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[i];

        // Create listener socket
        infra_socket_t listener = -1;
        infra_error_t err = infra_net_create(&listener, false);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to create listener for rule %d: %d", i, err);
            continue;
        }

        // Set socket options
        int optval = 1;
        err = infra_net_set_option(listener, INFRA_SOL_SOCKET, INFRA_SO_REUSEADDR, &optval, sizeof(optval));
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to set socket options for rule %d: %d", i, err);
            infra_net_close(listener);
            continue;
        }

        // Set non-blocking mode
        err = infra_net_set_nonblock(listener, true);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to set non-blocking mode for rule %d: %d", i, err);
            infra_net_close(listener);
            continue;
        }

        // Bind and listen
        infra_net_addr_t addr = {
            .port = rule->src_port
        };
        strncpy(addr.ip, rule->src_addr, sizeof(addr.ip) - 1);

        err = infra_net_bind(listener, &addr);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to bind %s:%d for rule %d: %d", 
                rule->src_addr, rule->src_port, i, err);
            infra_net_close(listener);
            continue;
        }

        err = infra_net_listen(listener, SOMAXCONN);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to listen on %s:%d for rule %d: %d", 
                rule->src_addr, rule->src_port, i, err);
            infra_net_close(listener);
            continue;
        }

        // Save listener
        rule->listener = listener;

        // Create accept thread
        err = infra_thread_create(&rule->thread, handle_connection, rule);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to create thread for rule %d: %d", i, err);
            infra_net_close(listener);
            continue;
        }

        INFRA_LOG_INFO("Started forwarding %s:%d -> %s:%d", 
            rule->src_addr, rule->src_port,
            rule->dst_addr, rule->dst_port);
    }

    g_rinetd_state.running = true;
    g_rinetd_service.state = PEER_SERVICE_STATE_RUNNING;
    return INFRA_OK;
}

// Stop rinetd service
infra_error_t rinetd_stop(void) {
    if (g_rinetd_service.state != PEER_SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_INVALID_STATE;
    }

    // Signal threads to stop
    g_rinetd_state.running = false;

    // Close all listeners and wait for threads
    for (int i = 0; i < g_rinetd_default_config.rules.count; i++) {
        rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[i];
        
        // Close listener to unblock accept
        if (rule->listener >= 0) {
            infra_net_close(rule->listener);
            rule->listener = -1;
        }

        // Wait for accept thread to finish
        infra_thread_join(rule->thread);
    }

    g_rinetd_service.state = PEER_SERVICE_STATE_STOPPED;
    return INFRA_OK;
}

// Clean up rinetd service
infra_error_t rinetd_cleanup(void) {
    g_rinetd_service.state = PEER_SERVICE_STATE_INIT;
    return INFRA_OK;
}

// Handle command
infra_error_t rinetd_cmd_handler(const char* cmd, char* response, size_t size) {
    if (!cmd || !response || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (strcmp(cmd, "start") == 0) {
        // First initialize if needed
        if (g_rinetd_service.state == PEER_SERVICE_STATE_INIT || 
            g_rinetd_service.state == PEER_SERVICE_STATE_STOPPED) {
            infra_error_t err = rinetd_init();
            if (err != INFRA_OK) {
                snprintf(response, size, "Failed to initialize service: %d\n", err);
                return err;
            }
        }
        
        // Then start the service
        infra_error_t err = rinetd_start();
        if (err == INFRA_OK) {
            snprintf(response, size, "Service started successfully\n");
        } else {
            snprintf(response, size, "Failed to start service: %d\n", err);
        }
        return err;
    } else if (strcmp(cmd, "stop") == 0) {
        infra_error_t err = rinetd_stop();
        if (err == INFRA_OK) {
            snprintf(response, size, "Service stopped successfully\n");
        } else {
            snprintf(response, size, "Failed to stop service: %d\n", err);
        }
        return err;
    } else if (strcmp(cmd, "status") == 0) {
        const char* state_str = "unknown";
        switch (g_rinetd_service.state) {
            case PEER_SERVICE_STATE_INIT:
                state_str = "initialized";
                break;
            case PEER_SERVICE_STATE_READY:
                state_str = "ready";
                break;
            case PEER_SERVICE_STATE_RUNNING:
                state_str = "running";
                break;
            case PEER_SERVICE_STATE_STOPPED:
                state_str = "stopped";
                break;
        }
        snprintf(response, size, "Service is %s\n", state_str);
        return INFRA_OK;
    }

    return INFRA_ERROR_NOT_SUPPORTED;
}

// Get rinetd service instance
peer_service_t* peer_rinetd_get_service(void) {
    return &g_rinetd_service;
}

// Load configuration from file
infra_error_t rinetd_load_config(const char* path) {
    FILE* fp = fopen(path, "r");
    if (!fp) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // Reset rules
    g_rinetd_default_config.rules.count = 0;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // Parse line
        char src_addr[64], dst_addr[64];
        int src_port, dst_port;
        if (sscanf(line, "%63s %d %63s %d", src_addr, &src_port, dst_addr, &dst_port) != 4) {
            INFRA_LOG_WARN("Invalid config line: %s", line);
            continue;
        }

        // Add forward rule
        if (g_rinetd_default_config.rules.count >= MAX_FORWARD_RULES) {
            INFRA_LOG_WARN("Too many forward rules, ignoring: %s", line);
            continue;
        }

        rinetd_rule_t* rule = &g_rinetd_default_config.rules.rules[g_rinetd_default_config.rules.count++];
        strncpy(rule->src_addr, src_addr, sizeof(rule->src_addr) - 1);
        rule->src_port = src_port;
        strncpy(rule->dst_addr, dst_addr, sizeof(rule->dst_addr) - 1);
        rule->dst_port = dst_port;

        INFRA_LOG_INFO("Added forward rule: %s:%d -> %s:%d", 
            src_addr, src_port, dst_addr, dst_port);
    }

    fclose(fp);
    return INFRA_OK;
}

// Save configuration to file
infra_error_t rinetd_save_config(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    // TODO: Implement configuration saving
    return INFRA_OK;
}
