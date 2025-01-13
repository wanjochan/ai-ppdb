#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/peer/peer_rinetd.h"

// Global options
static const poly_cmd_option_t global_options[] = {
    {"log-level", "Log level (0:none, 1:error, 2:warn, 3:info, 4:debug, 5:trace)", true},
};

static const int global_option_count = sizeof(global_options) / sizeof(global_options[0]);

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

    // Parse global options first (must be before command)
    char* log_level_str = NULL;
    int i;
    fprintf(stderr, "DEBUG: Parsing command line arguments, argc=%d\n", argc);
    for (i = 1; i < argc; i++) {
        fprintf(stderr, "DEBUG: Processing argument %d: %s\n", i, argv[i]);
        if (argv[i][0] != '-' || argv[i][1] != '-') {
            // Found command, stop parsing global options
            fprintf(stderr, "DEBUG: Found command: %s\n", argv[i]);
            break;
        }

        if (strncmp(argv[i], "--log-level=", 11) == 0) {
            log_level_str = argv[i] + 11;  // Skip "--log-level="
            if (*log_level_str == '\0') {  // Empty value
                fprintf(stderr, "ERROR: --log-level requires a value\n");
                fprintf(stderr, "Example: ppdb --log-level=4 help\n");
                help_cmd_handler(0, NULL);
                return 1;
            }
            fprintf(stderr, "DEBUG: Found log level option: %s\n", log_level_str);
        } else {
            fprintf(stderr, "ERROR: Unknown global option: %s\n", argv[i]);
            fprintf(stderr, "ERROR: Global options must come before command\n");
            fprintf(stderr, "ERROR: Example: ppdb --log-level=4 help\n");
            help_cmd_handler(0, NULL);
            return 1;
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
        // Skip leading '=' if present
        if (*log_level_str == '=') {
            log_level_str++;
        }
        
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
            fprintf(stderr, "DEBUG: Setting log level to %d\n", (int)level);
        } else {
            fprintf(stderr, "ERROR: Invalid log level: %ld (valid range: 0-5)\n", level);
            return 1;
        }
    } else {
        config.log.level = INFRA_LOG_LEVEL_NONE;  // Default to NONE
        fprintf(stderr, "DEBUG: Using default log level: NONE\n");
    }

    // Initialize infrastructure layer
    fprintf(stderr, "DEBUG: Initializing infrastructure layer\n");
    err = infra_init_with_config(INFRA_INIT_ALL, &config);
    if (err != INFRA_OK) {
        fprintf(stderr, "Failed to initialize infrastructure layer\n");
        return 1;
    }
    fprintf(stderr, "DEBUG: Infrastructure layer initialized\n");

    // Initialize command line framework
    fprintf(stderr, "DEBUG: Initializing command line framework\n");
    err = poly_cmdline_init();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize command line framework");
        return 1;
    }
    fprintf(stderr, "DEBUG: Command line framework initialized\n");

    // Register commands
    fprintf(stderr, "DEBUG: Registering help command\n");
    const poly_cmd_t help_cmd = {
        .name = "help",
        .desc = "Show help information",
        .handler = help_cmd_handler,
    };

    err = poly_cmdline_register(&help_cmd);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register help command");
        return 1;
    }
    fprintf(stderr, "DEBUG: Help command registered\n");

    // If no command specified, show help
    if (i >= argc) {
        INFRA_LOG_ERROR("No command specified");
        help_cmd_handler(0, NULL);
        return 1;
    }

    // Execute command (pass remaining arguments)
    fprintf(stderr, "DEBUG: Executing command: %s\n", argv[i]);
    err = poly_cmdline_execute(argc - i, argv + i);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Command execution failed: %s", infra_error_string(err));
        return 1;
    }
    fprintf(stderr, "DEBUG: Command executed successfully\n");

    return 0;
} 
