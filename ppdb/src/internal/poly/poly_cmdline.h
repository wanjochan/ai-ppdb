/**
 * @file poly_cmdline.h
 * @brief PPDB Command Line Framework
 */

#ifndef PPDB_POLY_CMDLINE_H
#define PPDB_POLY_CMDLINE_H

#include "internal/infra/infra_core.h"

#define POLY_CMD_MAX_NAME 32
#define POLY_CMD_MAX_DESC 256
#define POLY_CMD_MAX_ARGS 16

typedef struct poly_cmd_option {
    char name[POLY_CMD_MAX_NAME];
    char desc[POLY_CMD_MAX_DESC];
    bool has_value;
} poly_cmd_option_t;

typedef struct poly_cmd {
    char name[POLY_CMD_MAX_NAME];
    char desc[POLY_CMD_MAX_DESC];
    const poly_cmd_option_t *options;
    int option_count;
    infra_error_t (*handler)(int argc, char **argv);
} poly_cmd_t;

/**
 * @brief Initialize command line framework
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_init(void);

/**
 * @brief Register a command
 * @param cmd Command structure
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_register(const poly_cmd_t *cmd);

/**
 * @brief Execute command line
 * @param argc Argument count
 * @param argv Argument values
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_execute(int argc, char **argv);

/**
 * @brief Show help information
 * @param cmd_name Optional command name for detailed help
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_help(const char *cmd_name);

/**
 * @brief Get all registered commands
 * @param count Output parameter to store command count
 * @return Pointer to array of commands, or NULL if no commands registered
 */
const poly_cmd_t* poly_cmdline_get_commands(int* count);

/**
 * @brief Cleanup command line framework
 * @return infra_error_t Error code
 */
infra_error_t poly_cmdline_cleanup(void);

#endif /* PPDB_POLY_CMDLINE_H */ 