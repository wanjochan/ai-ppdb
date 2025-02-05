#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_log.h"
#include "internal/peer/peer_service.h"
#ifdef DEV_RINETD
#include "internal/peer/peer_rinetd.h"
#endif
#ifdef DEV_MEMKV
#include "internal/peer/peer_memkv.h"
#endif
#ifdef DEV_SQLITE3
#include "internal/peer/peer_sqlite3.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SERVICES 16
#define MAX_CMD_RESPONSE 4096

// Forward declarations
static const char* get_service_type_name(poly_service_type_t type);
static infra_error_t handle_rinetd_cmd(const poly_config_t* config, int argc, char** argv);

// Global service registry
typedef struct {
    peer_service_t* services[MAX_SERVICES];
    int service_count;
} service_registry_t;

static service_registry_t g_registry = {0};

// Register a service
infra_error_t peer_service_register(peer_service_t* service) {
    if (!service || !service->config.name[0]) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_registry.service_count >= MAX_SERVICES) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // Check if service already exists
    for (int i = 0; i < g_registry.service_count; i++) {
        if (strcmp(g_registry.services[i]->config.name, service->config.name) == 0) {
            return INFRA_ERROR_ALREADY_EXISTS;
        }
    }

    service->state = PEER_SERVICE_STATE_STOPPED;
    g_registry.services[g_registry.service_count++] = service;
    return INFRA_OK;
}

// Get service by name
peer_service_t* peer_service_get_by_name(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < g_registry.service_count; i++) {
        if (strcmp(g_registry.services[i]->config.name, name) == 0) {
            return g_registry.services[i];
        }
    }
    return NULL;
}

// Get service state
peer_service_state_t peer_service_get_state(const char* name) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return PEER_SERVICE_STATE_INIT;
    }
    return service->state;
}

// Apply service configuration
infra_error_t peer_service_apply_config(const char* name, const poly_service_config_t* config) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return INFRA_ERROR_NOT_FOUND;
    }

    if (service->apply_config) {
        return service->apply_config(config);
    }

    return INFRA_OK;
}

// Start service
infra_error_t peer_service_start(const char* name) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return INFRA_ERROR_NOT_FOUND;
    }

    if (service->state == PEER_SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Initialize if needed
    if (service->state == PEER_SERVICE_STATE_INIT) {
        infra_error_t err = service->init();
        if (err != INFRA_OK) {
            return err;
        }
    }

    // Start service
    return service->start();
}

// Stop service
infra_error_t peer_service_stop(const char* name) {
    peer_service_t* service = peer_service_get_by_name(name);
    if (!service) {
        return INFRA_ERROR_NOT_FOUND;
    }

    if (service->state != PEER_SERVICE_STATE_RUNNING) {
        return INFRA_OK;
    }

    return service->stop();
}

// Register commands
static void register_commands(void) {
    static const poly_cmd_option_t rinetd_options[] = {
        {"start", "Start the service in foreground", false},
        {"stop", "Stop the service", false},
        {"status", "Show service status", false},
        {"daemon", "Run as daemon in background", false},
        {"config", "Configuration file path", true},
        {"log-level", "Log level (0-5)", true}
    };

    static const poly_cmd_t commands[] = {
        {
            .name = "rinetd",
            .desc = "Manage rinetd service",
            .options = rinetd_options,
            .option_count = sizeof(rinetd_options) / sizeof(rinetd_options[0]),
            .handler = handle_rinetd_cmd
        }
    };

    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        poly_cmdline_register(&commands[i]);
    }
}

// Helper function to get service type name
static const char* get_service_type_name(poly_service_type_t type) {
    switch (type) {
        case POLY_SERVICE_RINETD: return "rinetd";
        case POLY_SERVICE_SQLITE: return "sqlite3";
        case POLY_SERVICE_MEMKV: return "memkv";
        case POLY_SERVICE_DISKV: return "diskv";
        default: return "unknown";
    }
}

// Handle rinetd command
static infra_error_t handle_rinetd_cmd(const poly_config_t* config, int argc, char** argv) {
    bool start_flag = false;
    bool stop_flag = false;
    bool status_flag = false;
    bool daemon_flag = false;

    // Parse command options
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            start_flag = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            stop_flag = true;
        } else if (strcmp(argv[i], "--status") == 0) {
            status_flag = true;
        } else if (strcmp(argv[i], "--daemon") == 0) {
            daemon_flag = true;
            start_flag = true;  // --daemon 隐含 --start
        }
    }

    // Default to status if no action specified
    if (!start_flag && !stop_flag && !status_flag) {
        status_flag = true;
    }

    // Handle actions
    if (start_flag) {
        // 如果配置文件中有该服务的配置，先应用配置
        bool has_config = false;
        for (int i = 0; i < config->service_count; i++) {
            const poly_service_config_t* svc_config = &config->services[i];
            if (svc_config->type == POLY_SERVICE_RINETD) {
                infra_error_t err = peer_service_apply_config("rinetd", svc_config);
                if (err != INFRA_OK) {
                    return err;
                }
                has_config = true;
            }
        }

        if (!has_config) {
            INFRA_LOG_ERROR("No rinetd configuration found");
            return INFRA_ERROR_NOT_FOUND;
        }

        // 启动服务
        infra_error_t err = peer_service_start("rinetd");
        if (err != INFRA_OK) {
            return err;
        }

        // 如果不是守护进程模式,则等待服务结束
        if (!daemon_flag) {
            peer_service_t* service = peer_service_get_by_name("rinetd");
            if (!service) {
                return INFRA_ERROR_NOT_FOUND;
            }

            // 等待服务结束或者收到中断信号
            while (service->state == PEER_SERVICE_STATE_RUNNING) {
                infra_sleep(100);  // 睡眠100ms
            }
        }

        return INFRA_OK;
    } else if (stop_flag) {
        return peer_service_stop("rinetd");
    } else {
        // Show status
        peer_service_t* service = peer_service_get_by_name("rinetd");
        if (!service) {
            printf("Service rinetd not found\n");
            return INFRA_ERROR_NOT_FOUND;
        }

        char response[MAX_CMD_RESPONSE];
        return service->cmd_handler("status", response, sizeof(response));
    }
}

// Main entry
int main(int argc, char** argv) {
    // Initialize infrastructure
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize infrastructure: %d\n", err);
        return 1;
    }

    // Initialize command line framework
    err = poly_cmdline_init();
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize command line framework: %d\n", err);
        return 1;
    }

    // Register commands
    register_commands();

    // Register services
#ifdef DEV_RINETD
    peer_service_register(peer_rinetd_get_service());
#endif
#ifdef DEV_MEMKV
    peer_service_register(peer_memkv_get_service());
#endif
#ifdef DEV_SQLITE3
    peer_service_register(peer_sqlite3_get_service());
#endif

    // Execute command
    err = poly_cmdline_execute(argc, argv);
    
    // Cleanup
    poly_cmdline_cleanup();
    infra_cleanup();
    
    return err == INFRA_OK ? 0 : 1;
}
