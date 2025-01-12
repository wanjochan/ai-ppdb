#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/peer/peer_rinetd.h"

static infra_error_t help_cmd_handler(int argc, char** argv) {
    int cmd_count = 0;
    const poly_cmd_t* commands = poly_cmdline_get_commands(&cmd_count);
    if (commands == NULL) {
        INFRA_LOG_ERROR("Failed to get commands");
        return INFRA_ERROR_NOT_FOUND;
    }

    INFRA_LOG_INFO("Usage: ppdb <command> [options]");
    INFRA_LOG_INFO("Available commands:");

    for (int i = 0; i < cmd_count; i++) {
        INFRA_LOG_INFO("  %-20s %s", commands[i].name, commands[i].desc);
        
        if (commands[i].options != NULL && commands[i].option_count > 0) {
            INFRA_LOG_INFO("    Options:");
            for (int j = 0; j < commands[i].option_count; j++) {
                INFRA_LOG_INFO("      --%s%s\t%s",
                    commands[i].options[j].name,
                    commands[i].options[j].has_value ? "=<value>" : "",
                    commands[i].options[j].desc);
            }
        }
    }

    return INFRA_OK;
}

int main(int argc, char** argv) {
    // Initialize infrastructure layer
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize infrastructure layer");
        return 1;
    }

    // Initialize command line framework
    err = poly_cmdline_init();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize command line framework");
        infra_cleanup();
        return 1;
    }

    // Register commands
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
        poly_cmdline_cleanup();
        infra_cleanup();
        return 1;
    }

    poly_cmd_t rinetd_cmd = {
        .name = "rinetd",
        .desc = "Port forwarding service",
        .options = rinetd_options,
        .option_count = sizeof(rinetd_options) / sizeof(rinetd_options[0]),
        .handler = rinetd_cmd_handler
    };

    err = poly_cmdline_register(&rinetd_cmd);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register rinetd command");
        poly_cmdline_cleanup();
        infra_cleanup();
        return 1;
    }

    // Execute command
    if (argc < 2) {
        help_cmd_handler(argc, argv);
        poly_cmdline_cleanup();
        infra_cleanup();
        return 0;
    }

    err = poly_cmdline_execute(argc - 1, argv + 1);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Command execution failed: %s", infra_error_str(err));
        poly_cmdline_cleanup();
        infra_cleanup();
        return 1;
    }

    // Cleanup
    poly_cmdline_cleanup();
    infra_cleanup();
    return 0;
} 
