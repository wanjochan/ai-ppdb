#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"

#define POLY_CMD_MAX_COUNT 32

typedef struct {
    poly_cmd_t commands[POLY_CMD_MAX_COUNT];
    int command_count;
} poly_cmdline_t;

static poly_cmdline_t g_cmdline;

infra_error_t poly_cmdline_init(void) {
    infra_memset(&g_cmdline, 0, sizeof(g_cmdline));
    return INFRA_OK;
}

void poly_cmdline_cleanup(void) {
    infra_memset(&g_cmdline, 0, sizeof(g_cmdline));
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
        if (infra_strcmp(g_cmdline.commands[i].name, cmd->name) == 0) {
            INFRA_LOG_ERROR("Command %s already exists", cmd->name);
            return INFRA_ERROR_EXISTS;
        }
    }

    infra_memcpy(&g_cmdline.commands[g_cmdline.command_count], cmd, sizeof(poly_cmd_t));
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
        if (infra_strcmp(g_cmdline.commands[i].name, cmd_name) == 0) {
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