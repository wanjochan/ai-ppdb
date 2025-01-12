#include "internal/peer/peer_rinetd.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_mux.h"
#include "internal/infra/infra_net.h"

#define MAX_RULES 64

typedef struct {
    char src_host[256];
    uint16_t src_port;
    char dst_host[256];
    uint16_t dst_port;
    infra_handle_t src_sock;
    infra_handle_t dst_sock;
} rinetd_rule_t;

typedef struct {
    rinetd_rule_t rules[MAX_RULES];
    int rule_count;
    bool running;
    void* mux;
} rinetd_context_t;

static rinetd_context_t g_rinetd;

const poly_cmd_option_t rinetd_options[] = {
    {"config", "Config file path", true},
    {"start", "Start the service", false},
};

static infra_error_t parse_config_file(const char* path) {
    if (path == NULL) {
        INFRA_LOG_ERROR("Invalid config file path");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO: Implement config file parsing
    INFRA_LOG_INFO("Parsing config file: %s", path);
    return INFRA_OK;
}

static infra_error_t handle_connection(infra_handle_t client_sock, const rinetd_rule_t* rule) {
    if (client_sock == NULL || rule == NULL) {
        INFRA_LOG_ERROR("Invalid parameters");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO: Implement connection handling
    INFRA_LOG_INFO("Handling connection for rule: %s:%d -> %s:%d",
        rule->src_host, rule->src_port,
        rule->dst_host, rule->dst_port);
    return INFRA_OK;
}

static infra_error_t start_service(void) {
    if (g_rinetd.running) {
        INFRA_LOG_ERROR("Service is already running");
        return INFRA_ERROR_INVALID_STATE;
    }

    g_rinetd.mux = infra_mux_create();
    if (g_rinetd.mux == NULL) {
        INFRA_LOG_ERROR("Failed to create multiplexer");
        return INFRA_ERROR_NO_MEMORY;
    }

    for (int i = 0; i < g_rinetd.rule_count; i++) {
        rinetd_rule_t* rule = &g_rinetd.rules[i];
        
        // Create source socket
        infra_error_t err = infra_net_listen(&rule->src_sock, rule->src_host, rule->src_port);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to create source socket for rule %d", i);
            continue;
        }

        // Add to multiplexer
        err = infra_mux_add(g_rinetd.mux, rule->src_sock, INFRA_MUX_READ);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to add source socket to multiplexer for rule %d", i);
            infra_net_close(rule->src_sock);
            continue;
        }

        INFRA_LOG_INFO("Rule %d: %s:%d -> %s:%d",
            i, rule->src_host, rule->src_port,
            rule->dst_host, rule->dst_port);
    }

    g_rinetd.running = true;
    INFRA_LOG_INFO("Service started");
    return INFRA_OK;
}

static infra_error_t stop_service(void) {
    if (!g_rinetd.running) {
        INFRA_LOG_ERROR("Service is not running");
        return INFRA_ERROR_INVALID_STATE;
    }

    for (int i = 0; i < g_rinetd.rule_count; i++) {
        rinetd_rule_t* rule = &g_rinetd.rules[i];
        if (rule->src_sock != NULL) {
            infra_net_close(rule->src_sock);
        }
        if (rule->dst_sock != NULL) {
            infra_net_close(rule->dst_sock);
        }
    }

    if (g_rinetd.mux != NULL) {
        infra_mux_destroy(g_rinetd.mux);
        g_rinetd.mux = NULL;
    }

    g_rinetd.running = false;
    INFRA_LOG_INFO("Service stopped");
    return INFRA_OK;
}

infra_error_t rinetd_cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        INFRA_LOG_ERROR("Invalid arguments");
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* cmd = argv[1];
    if (infra_strcmp(cmd, "start") == 0) {
        return start_service();
    } else if (infra_strcmp(cmd, "stop") == 0) {
        return stop_service();
    } else if (infra_strcmp(cmd, "config") == 0) {
        if (argc < 3) {
            INFRA_LOG_ERROR("Missing config file path");
            return INFRA_ERROR_INVALID_PARAM;
        }
        return parse_config_file(argv[2]);
    }

    INFRA_LOG_ERROR("Unknown command: %s", cmd);
    return INFRA_ERROR_INVALID_PARAM;
} 