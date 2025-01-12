#include "poly_cmdline.h"
#include "cosmopolitan.h"
#include "../infra/infra_error.h"
#include "../infra/infra_printf.h"

#define POLY_CMD_MAX_COUNT 32

static struct {
    poly_cmd_t commands[POLY_CMD_MAX_COUNT];
    int command_count;
} g_cmdline;

infra_error_t poly_cmdline_init(void)
{
    memset(&g_cmdline, 0, sizeof(g_cmdline));
    return INFRA_OK;
}

infra_error_t poly_cmdline_register(const poly_cmd_t *cmd)
{
    if (cmd == NULL) {
        return INFRA_ERROR_INVALID;
    }

    if (g_cmdline.command_count >= POLY_CMD_MAX_COUNT) {
        return INFRA_ERROR_NO_MEMORY;
    }

    memcpy(&g_cmdline.commands[g_cmdline.command_count], cmd, sizeof(poly_cmd_t));
    g_cmdline.command_count++;

    return INFRA_OK;
}

static const poly_cmd_t *find_command(const char *name)
{
    for (int i = 0; i < g_cmdline.command_count; i++) {
        if (strcmp(g_cmdline.commands[i].name, name) == 0) {
            return &g_cmdline.commands[i];
        }
    }
    return NULL;
}

infra_error_t poly_cmdline_help(const char *cmd_name)
{
    if (cmd_name == NULL) {
        infra_printf("Usage: ppdb <command> [options]\n\n");
        infra_printf("Available commands:\n");
        for (int i = 0; i < g_cmdline.command_count; i++) {
            infra_printf("  %-20s %s\n", 
                g_cmdline.commands[i].name, 
                g_cmdline.commands[i].desc);
        }
        return INFRA_OK;
    }

    const poly_cmd_t *cmd = find_command(cmd_name);
    if (cmd == NULL) {
        infra_printf("Unknown command: %s\n", cmd_name);
        return INFRA_ERROR_NOT_FOUND;
    }

    infra_printf("Command: %s\n", cmd->name);
    infra_printf("Description: %s\n\n", cmd->desc);
    
    if (cmd->options && cmd->option_count > 0) {
        infra_printf("Options:\n");
        for (int i = 0; i < cmd->option_count; i++) {
            infra_printf("  --%s%s\t%s\n",
                cmd->options[i].name,
                cmd->options[i].has_value ? "=<value>" : "",
                cmd->options[i].desc);
        }
    }

    return INFRA_OK;
}

infra_error_t poly_cmdline_execute(int argc, char **argv)
{
    if (argc < 2) {
        return poly_cmdline_help(NULL);
    }

    const char *cmd_name = argv[1];
    const poly_cmd_t *cmd = find_command(cmd_name);
    
    if (cmd == NULL) {
        infra_printf("Unknown command: %s\n", cmd_name);
        return INFRA_ERROR_NOT_FOUND;
    }

    if (cmd->handler == NULL) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    return cmd->handler(argc - 1, argv + 1);
}

infra_error_t poly_cmdline_cleanup(void)
{
    memset(&g_cmdline, 0, sizeof(g_cmdline));
    return INFRA_OK;
} 