#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/peer/peer_rinetd.h"

#define POLY_CMD_MAX_COUNT 32

typedef struct {
    poly_cmd_t commands[POLY_CMD_MAX_COUNT];
    int command_count;
} poly_cmdline_t;

static poly_cmdline_t g_cmdline;

infra_error_t poly_cmdline_init(void) {
    memset(&g_cmdline, 0, sizeof(g_cmdline));
    
    // Register rinetd command
    poly_cmd_t rinetd_cmd = {
        .name = "rinetd",
        .desc = "Rinetd service management",
        .options = rinetd_options,
        .option_count = rinetd_option_count,
        .handler = rinetd_cmd_handler
    };

    infra_error_t err = poly_cmdline_register(&rinetd_cmd);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to register rinetd command");
        return err;
    }
    
    return INFRA_OK;
}

infra_error_t poly_cmdline_cleanup(void) {
    memset(&g_cmdline, 0, sizeof(g_cmdline));
    return INFRA_OK;
}

infra_error_t poly_cmdline_register(const poly_cmd_t* cmd) {
    if (cmd == NULL) {
        INFRA_LOG_ERROR("Invalid command");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_cmdline.command_count >= POLY_CMD_MAX_COUNT) {
        INFRA_LOG_ERROR("Too many commands");
        return INFRA_ERROR_NO_MEMORY;
    }

    for (int i = 0; i < g_cmdline.command_count; i++) {
        if (strcmp(g_cmdline.commands[i].name, cmd->name) == 0) {
            INFRA_LOG_ERROR("Command %s already exists", cmd->name);
            return INFRA_ERROR_EXISTS;
        }
    }

    memcpy(&g_cmdline.commands[g_cmdline.command_count], cmd, sizeof(poly_cmd_t));
    g_cmdline.command_count++;

    INFRA_LOG_INFO("Command %s registered", cmd->name);
    return INFRA_OK;
}

infra_error_t poly_cmdline_execute(int argc, char** argv) {
    if (argc < 1 || argv == NULL) {
        INFRA_LOG_ERROR("Invalid arguments");
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* cmd_name = argv[0];
    for (int i = 0; i < g_cmdline.command_count; i++) {
        if (strcmp(g_cmdline.commands[i].name, cmd_name) == 0) {
            INFRA_LOG_INFO("Executing command %s", cmd_name);
            return g_cmdline.commands[i].handler(argc, argv);
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

    *count = g_cmdline.command_count;
    return g_cmdline.commands;
}

infra_error_t poly_cmdline_help(const char* cmd_name) {
    if (cmd_name == NULL) {
        // Show general help
        INFRA_LOG_INFO("Usage: ppdb <command> [options]");
        INFRA_LOG_INFO("Available commands:");

        for (int i = 0; i < g_cmdline.command_count; i++) {
            INFRA_LOG_INFO("  %-20s %s", g_cmdline.commands[i].name, g_cmdline.commands[i].desc);
        }
    } else {
        // Show command specific help
        for (int i = 0; i < g_cmdline.command_count; i++) {
            if (strcmp(g_cmdline.commands[i].name, cmd_name) == 0) {
                INFRA_LOG_INFO("Command: %s", cmd_name);
                INFRA_LOG_INFO("Description: %s", g_cmdline.commands[i].desc);
                
                if (g_cmdline.commands[i].options != NULL && g_cmdline.commands[i].option_count > 0) {
                    INFRA_LOG_INFO("Options:");
                    for (int j = 0; j < g_cmdline.commands[i].option_count; j++) {
                        INFRA_LOG_INFO("  --%s%s\t%s",
                            g_cmdline.commands[i].options[j].name,
                            g_cmdline.commands[i].options[j].has_value ? "=<value>" : "",
                            g_cmdline.commands[i].options[j].desc);
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