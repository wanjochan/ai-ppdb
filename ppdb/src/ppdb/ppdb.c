#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_log.h"
#include "internal/peer/peer_service.h"
#include "internal/peer/peer_rinetd.h"
#include "internal/peer/peer_memkv.h"
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

    // Initialize infrastructure with default configuration
    err = infra_init();
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize infrastructure: %d\n", err);
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

    // Parse command line
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    // Process global options
    int arg_index = 1;
    const char* config_path = NULL;
    while (arg_index < argc && strncmp(argv[arg_index], "--", 2) == 0) {
        if (strncmp(argv[arg_index], "--log-level=", 11) == 0) {
            int level = atoi(argv[arg_index] + 11);
            if (level < 0 || level > 5) {
                fprintf(stderr, "Invalid log level: %d\n", level);
                return 1;
            }
            infra_log_set_level(level);
        } else if (strncmp(argv[arg_index], "--config=", 9) == 0) {
            config_path = argv[arg_index] + 9;
        }
        arg_index++;
    }

    // Get service and command
    if (arg_index >= argc - 1) {
        print_usage(argv[0]);
        return 1;
    }

    const char* service_name = argv[arg_index++];
    const char* command = argv[arg_index++];

    // Get service
    peer_service_t* service = peer_service_get_by_name(service_name);
    if (!service) {
        fprintf(stderr, "Unknown service: %s\n", service_name);
        return 1;
    }

    // Load config if provided
    if (config_path && strcmp(service_name, "rinetd") == 0) {
        err = rinetd_load_config(config_path);
        if (err != INFRA_OK) {
            fprintf(stderr, "Failed to load config: %d\n", err);
            return 1;
        }
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
