#include "../internal/poly/poly_cmdline.h"
#include "../internal/infra/infra_printf.h"

static infra_error_t help_cmd_handler(int argc, char **argv)
{
    if (argc < 2) {
        return poly_cmdline_help(NULL);
    }
    return poly_cmdline_help(argv[1]);
}

static const poly_cmd_t help_cmd = {
    .name = "help",
    .desc = "Show help information for commands",
    .options = NULL,
    .option_count = 0,
    .handler = help_cmd_handler
};

int main(int argc, char **argv)
{
    infra_error_t err;

    // Initialize command line framework
    err = poly_cmdline_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize command line framework\n");
        return 1;
    }

    // Register help command
    err = poly_cmdline_register(&help_cmd);
    if (err != INFRA_OK) {
        infra_printf("Failed to register help command\n");
        return 1;
    }

    // Execute command
    err = poly_cmdline_execute(argc, argv);
    if (err != INFRA_OK && err != INFRA_ERROR_NOT_FOUND) {
        infra_printf("Command execution failed\n");
        return 1;
    }

    // Cleanup
    poly_cmdline_cleanup();
    return 0;
} 
