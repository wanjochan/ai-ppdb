#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/peer/peer_service.h"

#ifdef DEV_RINETD
// #include "internal/peer/peer_rinetd.h"
extern peer_service_t g_rinetd_service;
#endif

#ifdef DEV_MEMKV
// #include "internal/peer/peer_memkv.h"
extern peer_service_t g_memkv_service;
#endif

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
    err = peer_service_register(&g_rinetd_service);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register rinetd service: %d", err);
        return err;
    }
    INFRA_LOG_INFO("Registered rinetd service");
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
}

static infra_error_t process_global_options(int argc, char** argv, infra_config_t* config) {
    if (argc < 1 || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Parse global options first
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
                    return INFRA_ERROR_INVALID_PARAM;
                }

                // Parse the numeric value
                char* endptr;
                long level = strtol(value, &endptr, 10);
                
                // Check for conversion errors
                if (*endptr != '\0' || endptr == value) {
                    INFRA_LOG_ERROR("Invalid log level: %s (must be a number)", value);
                    return INFRA_ERROR_INVALID_PARAM;
                }
                
                // Check value range
                if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
                    config->log.level = (int)level;
                } else {
                    INFRA_LOG_ERROR("Invalid log level: %ld (valid range: 0-5)", level);
                    return INFRA_ERROR_INVALID_PARAM;
                }
            }
        }
    }

    return INFRA_OK;
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

    // Process global options
    err = process_global_options(argc, argv, &config);
    if (err != INFRA_OK) {
        return 1;
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
