#include "internal/infrax/InfraxCore.h"
#include "internal/polyx/PolyxCmdline.h"
#include "internal/polyx/PolyxService.h"
#include "internal/polyx/PolyxServiceCmd.h"

// Forward declarations
static InfraxError ppx_execute_command(PolyxCmdline* cmdline, InfraxI32 argc, char** argv);
static InfraxError handle_help_cmd(PolyxCmdline* cmdline, const polyx_config_t* config, 
                                    InfraxI32 argc, char** argv);

// Command execution
static InfraxError ppx_execute_command(PolyxCmdline* cmdline, InfraxI32 argc, char** argv) {
    InfraxCore* core = InfraxCoreClass.singleton();
    
    if (!cmdline || argc < 1 || !argv) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }

    // Parse command line arguments
    InfraxError err = PolyxCmdlineClass.parse_args(cmdline, argc, argv);
    if (!INFRAX_ERROR_IS_OK(err)) {
        return err;
    }

    // Find command name
    const char* cmd_name = NULL;
    for (InfraxI32 i = 1; i < argc; i++) {
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
        core->printf(core, "Unknown command: %s\n", cmd_name);
        return make_error(INFRAX_ERROR_NOT_FOUND, "Command not found");
    }

    return cmd->handler(&cmdline->config, argc, argv);
}

// Help command handler
static InfraxError handle_help_cmd(PolyxCmdline* cmdline, const polyx_config_t* config, 
                                    InfraxI32 argc, char** argv) {
    InfraxCore* core = InfraxCoreClass.singleton();
    
    const char* cmd_name = NULL;
    if (argc > 2) {
        cmd_name = argv[2];
    }

    if (cmd_name) {
        // Show specific command help
        const polyx_cmd_t* cmd = PolyxCmdlineClass.find_command(cmdline, cmd_name);
        if (!cmd) {
            core->printf(core, "Unknown command: %s\n", cmd_name);
            return make_error(INFRAX_ERROR_NOT_FOUND, "Command not found");
        }

        core->printf(core, "Command: %s\n", cmd->name);
        core->printf(core, "Description: %s\n", cmd->desc);
        if (cmd->options && cmd->option_count > 0) {
            core->printf(core, "Options:\n");
            for (InfraxI32 i = 0; i < cmd->option_count; i++) {
                core->printf(core, "  --%s%s\t%s\n",
                    cmd->options[i].name,
                    cmd->options[i].has_value ? "=<value>" : "",
                    cmd->options[i].desc);
            }
        }
    } else {
        // Show all commands
        core->printf(core, "Usage: ppx [options] <command> [command_options]\n");
        core->printf(core, "Available commands:\n");
        const polyx_cmd_t* cmds = PolyxCmdlineClass.get_commands(cmdline);
        InfraxSize cmd_count = PolyxCmdlineClass.get_command_count(cmdline);
        for (InfraxSize i = 0; i < cmd_count; i++) {
            core->printf(core, "  %-20s %s\n", cmds[i].name, cmds[i].desc);
        }
    }
    return make_error(INFRAX_ERROR_OK, NULL);
}

// Main entry point
int main(int argc, char** argv) {
    InfraxCore* core = InfraxCoreClass.singleton();
    
    // Create command line instance
    PolyxCmdline* cmdline = PolyxCmdlineClass.new();
    if (!cmdline) {
        core->printf(core, "Failed to create command line instance\n");
        return 1;
    }

    // Parse command line for log level
    InfraxError err = PolyxCmdlineClass.parse_args(cmdline, argc, argv);
    if (INFRAX_ERROR_IS_OK(err)) {
        char log_level_str[16] = {0};
        err = PolyxCmdlineClass.get_option(cmdline, "--log-level", log_level_str, sizeof(log_level_str));
        if (INFRAX_ERROR_IS_OK(err) && log_level_str[0]) {
            InfraxI32 level = core->atoi(core, log_level_str);
            if (level >= 0 && level <= 5) {  // TODO: Define log levels in Infrax
                // TODO: Set log level through Infrax
            }
        }
    }

    // Create service command instance
    PolyxServiceCmd* service_cmd = PolyxServiceCmdClass.new();
    if (!service_cmd) {
        core->printf(core, "Failed to create service command instance\n");
        PolyxCmdlineClass.free(cmdline);
        return 1;
    }

    // Register service commands
    err = PolyxServiceCmdClass.register_all(service_cmd);
    if (!INFRAX_ERROR_IS_OK(err)) {
        core->printf(core, "Failed to register service commands: %d\n", err.code);
        PolyxServiceCmdClass.free(service_cmd);
        PolyxCmdlineClass.free(cmdline);
        return 1;
    }

    // Execute command
    err = ppx_execute_command(cmdline, argc, argv);
    
    // Cleanup
    PolyxServiceCmdClass.free(service_cmd);
    PolyxCmdlineClass.free(cmdline);

    return INFRAX_ERROR_IS_OK(err) ? 0 : 1;
}
