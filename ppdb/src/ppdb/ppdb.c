#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/peer/peer_rinetd.h"
#include "internal/peer/peer_memkv.h"
// #include "internal/peer/peer_tccrun.h"

// Global options
static const poly_cmd_option_t global_options[] = {
    {"log-level", "Log level (0:none, 1:error, 2:warn, 3:info, 4:debug, 5:trace)", true},
};

static const int global_option_count = sizeof(global_options) / sizeof(global_options[0]);

// MemKV command options
static const poly_cmd_option_t memkv_options[] = {
    {"port", "Port to listen on (default: 11211)", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show service status", false},
};

static const int memkv_option_count = sizeof(memkv_options) / sizeof(memkv_options[0]);

static infra_error_t memkv_cmd_handler(int argc, char** argv) {
    uint16_t port = MEMKV_DEFAULT_PORT;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            port = (uint16_t)atoi(argv[i] + 7);
        }
    }

    // Check for action flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            if (memkv_is_running()) {
                infra_printf("MemKV service is already running\n");
                return INFRA_ERROR_ALREADY_EXISTS;
            }

            infra_config_t config = INFRA_DEFAULT_CONFIG;
            infra_error_t err = memkv_init(port, &config);
            if (err != INFRA_OK) {
                infra_printf("Failed to initialize MemKV service: %d\n", err);
                return err;
            }

            err = memkv_start();
            if (err != INFRA_OK) {
                infra_printf("Failed to start MemKV service: %d\n", err);
                memkv_cleanup();
                return err;
            }

            infra_printf("MemKV service started on port %d\n", port);
            return INFRA_OK;
        }

        if (strcmp(argv[i], "--stop") == 0) {
            if (!memkv_is_running()) {
                infra_printf("MemKV service is not running\n");
                return INFRA_ERROR_NOT_FOUND;
            }

            infra_error_t err = memkv_stop();
            if (err != INFRA_OK) {
                infra_printf("Failed to stop MemKV service: %d\n", err);
                return err;
            }

            err = memkv_cleanup();
            if (err != INFRA_OK) {
                infra_printf("Failed to cleanup MemKV service: %d\n", err);
                return err;
            }

            infra_printf("MemKV service stopped\n");
            return INFRA_OK;
        }

        if (strcmp(argv[i], "--status") == 0) {
            if (memkv_is_running()) {
                infra_printf("MemKV service is running on port %d\n", port);
            } else {
                infra_printf("MemKV service is not running\n");
            }
            return INFRA_OK;
        }
    }

    infra_printf("Error: Please specify --start, --stop, or --status\n");
    return INFRA_ERROR_INVALID_PARAM;
}

static infra_error_t help_cmd_handler(int argc, char** argv) {
    (void)argc;
    (void)argv;

    INFRA_LOG_DEBUG("Showing help information");

    infra_printf("Usage: ppdb [global options] <command> [options]\n");
    infra_printf("Global options:\n");
    for (int i = 0; i < global_option_count; i++) {
        infra_printf("  --%s=<value>  %s\n", 
            global_options[i].name,
            global_options[i].desc);
    }
    infra_printf("\nAvailable commands:\n");

    int cmd_count = 0;
    const poly_cmd_t* commands = poly_cmdline_get_commands(&cmd_count);
    if (commands == NULL) {
        INFRA_LOG_ERROR("Failed to get commands");
        return INFRA_ERROR_NOT_FOUND;
    }

    for (int i = 0; i < cmd_count; i++) {
        infra_printf("  %-20s %s\n", commands[i].name, commands[i].desc);
        if (commands[i].options && commands[i].option_count > 0) {
            infra_printf("    Options:\n");
            for (int j = 0; j < commands[i].option_count; j++) {
                if (commands[i].options[j].has_value) {
                    infra_printf("      --%s=<value>  %s\n",
                        commands[i].options[j].name,
                        commands[i].options[j].desc);
                } else {
                    infra_printf("      --%s   %s\n",
                        commands[i].options[j].name,
                        commands[i].options[j].desc);
                }
            }
        }
    }

    INFRA_LOG_DEBUG("Help information displayed");
    return INFRA_OK;
}

int main(int argc, char** argv) {
    infra_error_t err;
    const char* log_level_str = NULL;

    // Parse global options first
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--log-level=", 12) == 0) {
            log_level_str = argv[i] + 12;
        }
    }

    // Initialize infrastructure layer with custom log level
    infra_config_t config;
    err = infra_config_init(&config);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize config\n");
        return 1;
    }

    // Set log level if specified
    if (log_level_str) {
        // Parse the numeric value
        char* endptr;
        long level = strtol(log_level_str, &endptr, 10);
        
        // Check for conversion errors
        if (*endptr != '\0' || endptr == log_level_str) {
            fprintf(stderr, "ERROR: Invalid log level: %s (must be a number)\n", log_level_str);
            return 1;
        }
        
        // Check value range
        if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
            config.log.level = (int)level;
        } else {
            fprintf(stderr, "ERROR: Invalid log level: %ld (valid range: 0-5)\n", level);
            return 1;
        }
    }

    // Initialize infrastructure layer
    err = infra_init_with_config(INFRA_INIT_ALL, &config);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize infrastructure layer\n");
        return 1;
    }

    INFRA_LOG_DEBUG("Infrastructure layer initialized with log level %d", config.log.level);

    // Initialize command line framework
    err = poly_cmdline_init();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize command line framework");
        return 1;
    }

    INFRA_LOG_DEBUG("Command line framework initialized");

    // Parse global options first (must be before command)
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] != '-') {
            // Found command, stop parsing global options
            break;
        }

        const char* option = argv[i] + 2;  // Skip "--"
        const char* value = strchr(option, '=');
        if (value) {
            size_t name_len = value - option;
            value++;  // Skip '='
            
            if (strncmp(option, "log-level", name_len) == 0) {
                if (*value == '\0') {  // Empty value
                    fprintf(stderr, "ERROR: --log-level requires a numeric value (0-5)\n");
                    fprintf(stderr, "Example: ppdb --log-level=4 help\n");
                    help_cmd_handler(0, NULL);
                    return 1;
                }
                log_level_str = value;
            } else {
                fprintf(stderr, "ERROR: Unknown global option: %s\n", argv[i]);
                fprintf(stderr, "ERROR: Global options must come before command\n");
                fprintf(stderr, "ERROR: Example: ppdb --log-level=4 help\n");
                help_cmd_handler(0, NULL);
                return 1;
            }
        } else {
            fprintf(stderr, "ERROR: Invalid option format: %s\n", argv[i]);
            fprintf(stderr, "ERROR: Global options must have a value\n");
            fprintf(stderr, "ERROR: Example: ppdb --log-level=4 help\n");
            help_cmd_handler(0, NULL);
            return 1;
        }
    }

    // Register commands (after infra_init)
    poly_cmd_t help_cmd = {
        .name = "help",
        .desc = "Show help information",
        .options = NULL,
        .option_count = 0,
        .handler = help_cmd_handler
    };
    err = poly_cmdline_register(&help_cmd);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register help command");
        return 1;
    }
    INFRA_LOG_DEBUG("Help command registered");

    // Register memkv command
    poly_cmd_t memkv_cmd = {
        .name = "memkv",
        .desc = "MemKV service management",
        .options = memkv_options,
        .option_count = memkv_option_count,
        .handler = memkv_cmd_handler
    };
    err = poly_cmdline_register(&memkv_cmd);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register memkv command");
        return 1;
    }
    INFRA_LOG_DEBUG("MemKV command registered");

    poly_cmd_t rinetd_cmd = {
        .name = "rinetd",
        .desc = "Rinetd service management",
        .options = rinetd_options,
        .option_count = rinetd_option_count,
        .handler = rinetd_cmd_handler
    };
    err = poly_cmdline_register(&rinetd_cmd);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register rinetd command: %d", err);
        return 1;
    }
    INFRA_LOG_DEBUG("Rinetd command registered");

    // If no command specified, show help
    if (i >= argc) {
        INFRA_LOG_ERROR("No command specified");
        help_cmd_handler(0, NULL);
        return 1;
    }

    // Execute command (pass remaining arguments)
    INFRA_LOG_DEBUG("Executing command: %s", argv[i]);
    err = poly_cmdline_execute(argc - i, argv + i);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Command execution failed: %s", infra_error_string(err));
        return 1;
    }
    INFRA_LOG_DEBUG("Command executed successfully");

    return 0;
} 
