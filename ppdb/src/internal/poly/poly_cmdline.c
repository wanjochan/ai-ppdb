#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"

// 命令列表
static poly_cmd_t* g_commands = NULL;
static int g_command_count = 0;
static int g_command_capacity = 0;

infra_error_t poly_cmdline_init(void)
{
    INFRA_LOG_DEBUG("Initializing command line framework");

    // 初始化命令列表
    g_command_capacity = 16;  // 初始容量
    g_commands = infra_malloc(g_command_capacity * sizeof(poly_cmd_t));
    if (!g_commands) {
        INFRA_LOG_ERROR("Failed to allocate command list");
        return INFRA_ERROR_NO_MEMORY;
    }

    INFRA_LOG_DEBUG("Command line framework initialized successfully");
    return INFRA_OK;
}

infra_error_t poly_cmdline_cleanup(void) {
    if (g_commands) {
        infra_free(g_commands);
        g_commands = NULL;
    }
    g_command_count = 0;
    g_command_capacity = 0;
    return INFRA_OK;
}

infra_error_t poly_cmdline_register(const poly_cmd_t* cmd) {
    INFRA_LOG_DEBUG("Registering command: %s", cmd ? cmd->name : "NULL");
    if (cmd == NULL) {
        INFRA_LOG_ERROR("Invalid command");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_command_count >= g_command_capacity) {
        INFRA_LOG_ERROR("Too many commands");
        return INFRA_ERROR_NO_MEMORY;
    }

    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, cmd->name) == 0) {
            INFRA_LOG_ERROR("Command %s already exists", cmd->name);
            return INFRA_ERROR_EXISTS;
        }
    }

    memcpy(&g_commands[g_command_count], cmd, sizeof(poly_cmd_t));
    g_command_count++;

    INFRA_LOG_INFO("Command %s registered", cmd->name);
    INFRA_LOG_DEBUG("Command count: %d", g_command_count);
    return INFRA_OK;
}

infra_error_t poly_cmdline_execute(int argc, char** argv) {
    INFRA_LOG_DEBUG("Executing command with argc=%d", argc);
    if (argc < 1 || argv == NULL) {
        INFRA_LOG_ERROR("Invalid arguments");
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* cmd_name = argv[0];
    INFRA_LOG_DEBUG("Looking for command: %s", cmd_name);
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, cmd_name) == 0) {
            INFRA_LOG_INFO("Executing command %s", cmd_name);
            INFRA_LOG_DEBUG("Calling command handler");
            return g_commands[i].handler(argc, argv);
        }
    }

    INFRA_LOG_ERROR("Command %s not found", cmd_name);
    return INFRA_ERROR_NOT_FOUND;
}

const poly_cmd_t* poly_cmdline_get_commands(int* count) {
    if (count == NULL) {
        INFRA_LOG_ERROR("Invalid count pointer");
        return NULL;
    }

    *count = g_command_count;
    return g_commands;
}

infra_error_t poly_cmdline_help(const char* cmd_name) {
    if (cmd_name == NULL) {
        // Show general help
        INFRA_LOG_INFO("Usage: ppdb <command> [options]");
        INFRA_LOG_INFO("Available commands:");

        for (int i = 0; i < g_command_count; i++) {
            INFRA_LOG_INFO("  %-20s %s", g_commands[i].name, g_commands[i].desc);
        }
    } else {
        // Show command specific help
        for (int i = 0; i < g_command_count; i++) {
            if (strcmp(g_commands[i].name, cmd_name) == 0) {
                INFRA_LOG_INFO("Command: %s", cmd_name);
                INFRA_LOG_INFO("Description: %s", g_commands[i].desc);
                
                if (g_commands[i].options != NULL && g_commands[i].option_count > 0) {
                    INFRA_LOG_INFO("Options:");
                    for (int j = 0; j < g_commands[i].option_count; j++) {
                        INFRA_LOG_INFO("  --%s%s\t%s",
                            g_commands[i].options[j].name,
                            g_commands[i].options[j].has_value ? "=<value>" : "",
                            g_commands[i].options[j].desc);
                    }
                }
                return INFRA_OK;
            }
        }
        INFRA_LOG_ERROR("Command %s not found", cmd_name);
        return INFRA_ERROR_NOT_FOUND;
    }
    return INFRA_OK;
} 