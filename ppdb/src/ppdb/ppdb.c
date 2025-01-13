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

    return INFRA_OK;
}

int main(int argc, char** argv) {
    // Initialize infrastructure layer first
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize infrastructure layer");
        return 1;
    }

    // Parse global options first (must be before command)
    char* log_level_str = NULL;
    int i;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] != '-') {
            // Found command, stop parsing global options
            break;
        }

        if (strncmp(argv[i], "--log-level=", 11) == 0) {
            log_level_str = argv[i] + 11;
        } else {
            INFRA_LOG_ERROR("Unknown global option: %s", argv[i]);
            INFRA_LOG_ERROR("Use 'help' command to see available options");
            return 1;
        }
    }

    // Set log level if specified
    if (log_level_str) {
        int level = atoi(log_level_str);
        if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
            infra_log_set_level(level);
        } else {
            INFRA_LOG_ERROR("Invalid log level: %s (valid range: 0-5)", log_level_str);
            return 1;
        }
    }

    // Initialize command line framework
    err = poly_cmdline_init();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize command line framework");
        return 1;
    }

    // Register commands
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

    // If no command specified, show help
    if (i >= argc) {
        INFRA_LOG_ERROR("No command specified");
        help_cmd_handler(0, NULL);
        return 1;
    }

    // Execute command (pass remaining arguments)
    err = poly_cmdline_execute(argc - i, argv + i);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Command execution failed: %s", infra_error_string(err));
        return 1;
    }

    return 0;
} 
