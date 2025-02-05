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

// Print usage information
static void print_usage(const char* program) {
    printf("Usage: %s [global options] <service> <command> [command_options]\n\n", program);
    printf("Global options:\n");
    printf("  --log-level=<level>   Set log level (0-5)\n");
    printf("  --config=<path>      Load config from file\n");
    printf("\nAvailable services:\n");
    
    for (int i = 0; i < g_registry.service_count; i++) {
        printf("  %s\n", g_registry.services[i]->config.name);
    }
    
    printf("\nCommon commands:\n");
    printf("  start     Start the service\n");
    printf("  stop      Stop the service\n");
    printf("  status    Show service status\n");
}

int main(int argc, char* argv[]) {
    infra_error_t err;
    int log_level = INFRA_LOG_LEVEL_INFO;  // 默认日志级别
    const char* config_path = NULL;
    const char* service_name = NULL;
    const char* command = NULL;

    // 定义全局选项
    static const poly_cmd_option_t global_options[] = {
        {"log-level", "Set log level (0-5)", true},
        {"config", "Load config from file", true},
        {"help", "Show help information", false},
        {"", "", false}  // 结束标记
    };

    // 初始化命令行框架
    err = poly_cmdline_init();
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize command line framework\n");
        return 1;
    }

    // 处理全局选项
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0) {
            const char* option = argv[i] + 2;
            const char* value = strchr(option, '=');
            char option_name[POLY_CMD_MAX_NAME];
            
            if (value) {
                size_t name_len = value - option;
                strncpy(option_name, option, name_len);
                option_name[name_len] = '\0';
                value++;  // 跳过等号
            } else {
                strncpy(option_name, option, sizeof(option_name) - 1);
                option_name[sizeof(option_name) - 1] = '\0';
                value = NULL;
            }

            // 查找选项
            for (int j = 0; global_options[j].name[0]; j++) {
                if (strcmp(option_name, global_options[j].name) == 0) {
                    if (strcmp(option_name, "log-level") == 0 && value) {
                        log_level = atoi(value);
                        if (log_level < 0 || log_level > 5) {
                            fprintf(stderr, "Invalid log level: %d\n", log_level);
                            return 1;
                        }
                    } else if (strcmp(option_name, "config") == 0 && value) {
                        config_path = value;
                    } else if (strcmp(option_name, "help") == 0) {
                        print_usage(argv[0]);
                        return 0;
                    }
                    break;
                }
            }
        } else if (!service_name) {
            service_name = argv[i];
        } else if (!command) {
            command = argv[i];
        }
    }

    // 检查必需的参数
    if (!service_name || !command) {
        print_usage(argv[0]);
        return 1;
    }

    // Initialize logging with the specified level
    infra_core_set_log_level(log_level);
    err = infra_log_init(log_level, NULL);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize logging: %d\n", err);
        return 1;
    }

    // Initialize infrastructure
    err = infra_init();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize infrastructure: %d", err);
        return 1;
    }

    // Register services
    err = peer_service_register(peer_rinetd_get_service());
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register rinetd service: %d", err);
        return 1;
    }

#ifdef DEV_MEMKV
    err = peer_service_register(peer_memkv_get_service());
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register memkv service: %d", err);
        return 1;
    }
#endif

#ifdef DEV_SQLITE3
    err = peer_service_register(&g_sqlite3_service);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register sqlite3 service: %d", err);
        return 1;
    }
#endif

    // Get service
    peer_service_t* service = peer_service_get_by_name(service_name);
    if (!service) {
        fprintf(stderr, "Unknown service: %s\n", service_name);
        return 1;
    }

    // Load config if provided
    if (config_path) {
        if (strcmp(service_name, "rinetd") == 0) {
            err = rinetd_load_config(config_path);
            if (err != INFRA_OK) {
                fprintf(stderr, "Failed to load config: %d\n", err);
                return 1;
            }
        }
#ifdef DEV_SQLITE3
        else if (strcmp(service_name, "sqlite3") == 0) {
            // Initialize service first
            err = sqlite3_init();
            if (err != INFRA_OK) {
                fprintf(stderr, "Failed to initialize sqlite3 service: %d\n", err);
                return 1;
            }

            // Build command with config path
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "start --config=%s", config_path);
            char resp[MAX_CMD_RESPONSE];
            err = service->cmd_handler(cmd, resp, sizeof(resp));
            if (err != INFRA_OK) {
                fprintf(stderr, "%s\n", resp);
                return 1;
            }
            printf("%s\n", resp);
            return 0;
        }
#endif
    }

    // Handle command
    char response[MAX_CMD_RESPONSE];
    err = service->cmd_handler(command, response, sizeof(response));
    
    // Print response if any
    if (response[0] != '\0') {
        printf("%s", response);
    }

    // Exit with error if command failed
    if (err != INFRA_OK) {
        return 1;
    }

    // If this is a start command, keep running
    if (strcmp(command, "start") == 0) {
        // Wait for user input to stop
        printf("Press Enter to stop the service...\n");
        getchar();

        // Stop the service
        err = service->cmd_handler("stop", response, sizeof(response));
        if (err != INFRA_OK) {
            fprintf(stderr, "Failed to stop service: %d\n", err);
            return 1;
        }

        // Print response if any
        if (response[0] != '\0') {
            printf("%s", response);
        }
    }

    return 0;
}
