#include <cosmopolitan.h>
#include "internal/infrax/InfraxCore.h"
#include "internal/polyx/PolyxAsync.h"
#include "internal/polyx/PolyxCmdline.h"
#include "internal/polyx/PolyxService.h"
#include "internal/polyx/PolyxServiceCmd.h"

// Forward declarations
static infrax_error_t ppx_execute_command(PolyxCmdline* cmdline, int argc, char** argv);
static infrax_error_t handle_help_cmd(PolyxCmdline* cmdline, const polyx_config_t* config, 
                                    int argc, char** argv);

// Command execution
static infrax_error_t ppx_execute_command(PolyxCmdline* cmdline, int argc, char** argv) {
    if (!cmdline || argc < 1 || !argv) {
        return INFRAX_ERROR_INVALID_PARAM;
    }

    // Parse command line arguments
    infrax_error_t err = PolyxCmdlineClass.parse_args(cmdline, argc, argv);
    if (err != INFRAX_OK) {
        return err;
    }

    // Find command name
    const char* cmd_name = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            cmd_name = argv[i];
            break;
        }
    }

    if (!cmd_name) {
        // Show help if no command
        return handle_help_cmd(cmdline, &cmdline->config, argc, argv);
    }

    // Find and execute command
    const polyx_cmd_t* cmd = PolyxCmdlineClass.find_command(cmdline, cmd_name);
    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n", cmd_name);
        return INFRAX_ERROR_NOT_FOUND;
    }

    return cmd->handler(&cmdline->config, argc, argv);
}

// Help command handler
static infrax_error_t handle_help_cmd(PolyxCmdline* cmdline, const polyx_config_t* config, 
                                    int argc, char** argv) {
    const char* cmd_name = NULL;
    if (argc > 2) {
        cmd_name = argv[2];
    }

    if (cmd_name) {
        // Show specific command help
        const polyx_cmd_t* cmd = PolyxCmdlineClass.find_command(cmdline, cmd_name);
        if (!cmd) {
            fprintf(stderr, "Unknown command: %s\n", cmd_name);
            return INFRAX_ERROR_NOT_FOUND;
        }

        printf("Command: %s\n", cmd->name);
        printf("Description: %s\n", cmd->desc);
        if (cmd->options && cmd->option_count > 0) {
            printf("Options:\n");
            for (int i = 0; i < cmd->option_count; i++) {
                printf("  --%s%s\t%s\n",
                    cmd->options[i].name,
                    cmd->options[i].has_value ? "=<value>" : "",
                    cmd->options[i].desc);
            }
        }
    } else {
        // Show all commands
        printf("Usage: ppx [options] <command> [command_options]\n");
        printf("Available commands:\n");
        const polyx_cmd_t* cmds = PolyxCmdlineClass.get_commands(cmdline);
        size_t cmd_count = PolyxCmdlineClass.get_command_count(cmdline);
        for (size_t i = 0; i < cmd_count; i++) {
            printf("  %-20s %s\n", cmds[i].name, cmds[i].desc);
        }
    }
    return INFRAX_OK;
}

// Main entry point
int main(int argc, char** argv) {
    // Initialize infrastructure
    infrax_error_t err = infrax_init();
    if (err != INFRAX_OK) {
        fprintf(stderr, "Failed to initialize infrastructure: %d\n", err);
        return 1;
    }

    // Create command line instance
    PolyxCmdline* cmdline = PolyxCmdlineClass.new();
    if (!cmdline) {
        fprintf(stderr, "Failed to create command line instance\n");
        infrax_cleanup();
        return 1;
    }

    // Parse command line for log level
    err = PolyxCmdlineClass.parse_args(cmdline, argc, argv);
    if (err == INFRAX_OK) {
        char log_level_str[16] = {0};
        err = PolyxCmdlineClass.get_option(cmdline, "--log-level", log_level_str, sizeof(log_level_str));
        if (err == INFRAX_OK && log_level_str[0]) {
            int level = atoi(log_level_str);
            if (level >= INFRAX_LOG_LEVEL_NONE && level <= INFRAX_LOG_LEVEL_TRACE) {
                infrax_log_set_level(level);
            }
        }
    }

    // Create service command instance
    PolyxServiceCmd* service_cmd = PolyxServiceCmdClass.new();
    if (!service_cmd) {
        fprintf(stderr, "Failed to create service command instance\n");
        PolyxCmdlineClass.free(cmdline);
        infrax_cleanup();
        return 1;
    }

    // Register service commands
    err = PolyxServiceCmdClass.register_all(service_cmd);
    if (err != INFRAX_OK) {
        fprintf(stderr, "Failed to register service commands: %d\n", err);
        PolyxServiceCmdClass.free(service_cmd);
        PolyxCmdlineClass.free(cmdline);
        infrax_cleanup();
        return 1;
    }

    // Execute command
    err = ppx_execute_command(cmdline, argc, argv);
    
    // Cleanup
    PolyxServiceCmdClass.free(service_cmd);
    PolyxCmdlineClass.free(cmdline);
    infrax_cleanup();

    return err == INFRAX_OK ? 0 : 1;
}
