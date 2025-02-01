#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"

#ifdef DEV_RINETD
#include "internal/peer/peer_rinetd.h"
#endif

#ifdef DEV_MEMKV
#include "internal/peer/peer_memkv.h"
#endif

#ifdef DEV_SQLITE3
#include "internal/peer/peer_sqlite3.h"
#endif

//-----------------------------------------------------------------------------
// Service Registry Implementation
//-----------------------------------------------------------------------------

// 服务注册表
static struct {
    peer_service_t* services[SERVICE_TYPE_COUNT];
    bool initialized;
} g_registry = {0};

// 注册服务
infra_error_t peer_service_register(peer_service_t* service) {
    if (!service) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (service->config.type <= SERVICE_TYPE_UNKNOWN || 
        service->config.type >= SERVICE_TYPE_COUNT) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_registry.services[service->config.type]) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    g_registry.services[service->config.type] = service;
    INFRA_LOG_INFO("Registered service: %s", service->config.name);

    return INFRA_OK;
}

// 获取服务
peer_service_t* peer_service_get_by_type(peer_service_type_t type) {
    if (type <= SERVICE_TYPE_UNKNOWN || type >= SERVICE_TYPE_COUNT) {
        return NULL;
    }
    return g_registry.services[type];
}

peer_service_t* peer_service_get(const char* name) {
    if (!name) {
        return NULL;
    }

    for (int i = SERVICE_TYPE_UNKNOWN + 1; i < SERVICE_TYPE_COUNT; i++) {
        peer_service_t* service = g_registry.services[i];
        if (service && strcmp(service->config.name, name) == 0) {
            return service;
        }
    }

    return NULL;
}

// 获取服务名称
const char* peer_service_get_name(peer_service_type_t type) {
    peer_service_t* service = peer_service_get_by_type(type);
    return service ? service->config.name : NULL;
}

// 获取服务状态
peer_service_state_t peer_service_get_state(peer_service_type_t type) {
    peer_service_t* service = peer_service_get_by_type(type);
    return service ? service->state : SERVICE_STATE_STOPPED;
} 

//-----------------------------------------------------------------------------
// Global Options
//-----------------------------------------------------------------------------

static const poly_cmd_option_t global_options[] = {
    {"log-level", "Log level (0:none, 1:error, 2:warn, 3:info, 4:debug, 5:trace)", true},
};

static const int global_option_count = sizeof(global_options) / sizeof(global_options[0]);

//-----------------------------------------------------------------------------
// Service Registry
//-----------------------------------------------------------------------------

static infra_error_t register_services(void) {
    infra_error_t err=INFRA_OK;
#ifdef DEV_MEMKV
    // Register MemKV service
    err = peer_service_register(&g_memkv_service);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register memkv service: %d", err);
        return err;
    }
    INFRA_LOG_INFO("Registered memkv service");
#endif

#ifdef DEV_RINETD
    // Register Rinetd service
    if ((err = peer_service_register(&g_rinetd_service)) != INFRA_OK) {
        return err;
    }
#endif

#ifdef DEV_SQLITE3
    // Register SQLite3 service
    err = peer_service_register(&g_sqlite3_service);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register sqlite3 service: %d", err);
        return err;
    }
    INFRA_LOG_INFO("Registered sqlite3 service");
#endif

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Command Line Processing
//-----------------------------------------------------------------------------

static void print_usage(const char* program) {
    infra_printf("Usage: %s [global options] <command> [command_options]\n\n", program);
    
    // Print global options
    infra_printf("Global options:\n");
    for (int i = 0; i < global_option_count; i++) {
        infra_printf("  --%s=<value>  %s\n", 
            global_options[i].name,
            global_options[i].desc);
    }

    // Print available commands
    infra_printf("\nAvailable commands:\n");
    infra_printf("  help    - Show help information\n");

#ifdef DEV_MEMKV
    infra_printf("  memkv   - MemKV service management\n");
    infra_printf("    Options:\n");
    infra_printf("      --port=<value>  Port to listen on (default: 11211)\n");
    infra_printf("      --start         Start the service\n");
    infra_printf("      --stop          Stop the service\n");
    infra_printf("      --status        Show service status\n");
#endif

#ifdef DEV_RINETD
    infra_printf("  rinetd  - Rinetd service management\n");
    infra_printf("    Options:\n");
    infra_printf("      --config=<value>  Configuration file path\n");
    infra_printf("      --start           Start the service\n");
    infra_printf("      --stop            Stop the service\n");
    infra_printf("      --status          Show service status\n");
#endif

#ifdef DEV_SQLITE3
    infra_printf("  sqlite3 - SQLite3 service management\n");
    infra_printf("    Options:\n");
    infra_printf("      --db=<value>     Database file path\n");
    infra_printf("      --port=<value>   Listen port (default: 5433)\n");
    infra_printf("      --start          Start the service\n");
    infra_printf("      --stop           Stop the service\n");
    infra_printf("      --status         Show service status\n");
    infra_printf("      --help           Show this help message\n");
#endif
}

//-----------------------------------------------------------------------------
// Main Entry
//-----------------------------------------------------------------------------

int main(int argc, char** argv) {
    infra_error_t err;

    // Initialize infrastructure layer with custom log level
    infra_config_t config;
    err = infra_config_init(&config);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize config\n");
        return 1;
    }

    // Process command line options
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] != '-') {
            continue;
        }

        const char* option = argv[i] + 2;  // Skip "--"
        const char* value = strchr(option, '=');
        if (value) {
            size_t name_len = value - option;
            value++;  // Skip '='

            if (strncmp(option, "log-level", name_len) == 0) {
                if (*value == '\0') {  // Empty value
                    INFRA_LOG_ERROR("--log-level requires a numeric value (0-5)");
                    INFRA_LOG_ERROR("Example: ppdb --log-level=4 help");
                    return 1;
                }

                // Parse the numeric value
                char* endptr;
                long level = strtol(value, &endptr, 10);

                // Check for conversion errors
                if (*endptr != '\0' || endptr == value) {
                    INFRA_LOG_ERROR("Invalid log level: %s (must be a number)", value);
                    return 1;
                }

                // Check value range
                if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
                    config.log.level = (int)level;
                } else {
                    INFRA_LOG_ERROR("Invalid log level: %ld (valid range: 0-5)", level);
                    return 1;
                }
            }
        }
    }

    // Initialize infrastructure layer
    err = infra_init_with_config(INFRA_INIT_ALL, &config);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize infrastructure layer\n");
        return 1;
    }

    INFRA_LOG_DEBUG("Infrastructure layer initialized with log level %d", config.log.level);

    // Register services
    err = register_services();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register services: %d", err);
        return 1;
    }

    // Find first non-option argument as command
    const char* cmd_name = NULL;
    int cmd_start = 1;
    for (; cmd_start < argc; cmd_start++) {
        if (argv[cmd_start][0] != '-') {
            cmd_name = argv[cmd_start];
            break;
        }
        // Skip value of --log-level if specified without =
        if (strcmp(argv[cmd_start], "--log-level") == 0 && cmd_start + 1 < argc) {
            cmd_start++;
        }
    }

    // If no command specified or help requested, show help
    if (!cmd_name || strcmp(cmd_name, "help") == 0) {
        print_usage(argv[0]);
        return !cmd_name ? 1 : 0;
    }

    // Find service by name
    peer_service_t* service = peer_service_get(cmd_name);
    if (!service) {
        INFRA_LOG_ERROR("Unknown command: %s", cmd_name);
        return 1;
    }

    // Execute command
    int cmd_argc = argc - cmd_start;
    char** cmd_argv = argv + cmd_start;
    err = service->cmd_handler(cmd_argc, cmd_argv);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Command failed: %d", err);
        return 1;
    }

    INFRA_LOG_DEBUG("Command executed successfully");
    return 0;
} 
