#include "internal/poly/poly_cmdline.h"
#include "internal/infra/infra_core.h"
#include "internal/peer/peer_rinetd.h"
#include "internal/peer/peer_memkv.h"
#include "ppdb/ppdb.h"
#include "internal/poly/poly_memkv_cmd.h"
// #include "internal/peer/peer_tccrun.h"

// Global options
static const poly_cmd_option_t global_options[] = {
    {"log-level", "Log level (0:none, 1:error, 2:warn, 3:info, 4:debug, 5:trace)", true},
};

static const int global_option_count = sizeof(global_options) / sizeof(global_options[0]);

// MemKV command options
static const poly_cmd_option_t memkv_options[] = {
    {"port", "Port to listen on (default: 11211)", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show service status", false},
};

static const int memkv_option_count = sizeof(memkv_options) / sizeof(memkv_options[0]);

static infra_error_t memkv_cmd_handler(int argc, char** argv) {
    uint16_t port = MEMKV_DEFAULT_PORT;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            port = (uint16_t)atoi(argv[i] + 7);
        }
    }

    // Check for action flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            if (memkv_is_running()) {
                infra_printf("MemKV service is already running\n");
                return INFRA_ERROR_ALREADY_EXISTS;
            }

            infra_config_t config = INFRA_DEFAULT_CONFIG;
            infra_error_t err = memkv_init(port, &config);
            if (err != INFRA_OK) {
                infra_printf("Failed to initialize MemKV service: %d\n", err);
                return err;
            }

            err = memkv_start();
            if (err != INFRA_OK) {
                infra_printf("Failed to start MemKV service: %d\n", err);
                memkv_cleanup();
                return err;
            }

            infra_printf("MemKV service started on port %d\n", port);
            return INFRA_OK;
        }

        if (strcmp(argv[i], "--stop") == 0) {
            if (!memkv_is_running()) {
                infra_printf("MemKV service is not running\n");
                return INFRA_ERROR_NOT_FOUND;
            }

            infra_error_t err = memkv_stop();
            if (err != INFRA_OK) {
                infra_printf("Failed to stop MemKV service: %d\n", err);
                return err;
            }

            err = memkv_cleanup();
            if (err != INFRA_OK) {
                infra_printf("Failed to cleanup MemKV service: %d\n", err);
                return err;
            }

            infra_printf("MemKV service stopped\n");
            return INFRA_OK;
        }

        if (strcmp(argv[i], "--status") == 0) {
            if (memkv_is_running()) {
                infra_printf("MemKV service is running on port %d\n", port);
            } else {
                infra_printf("MemKV service is not running\n");
            }
            return INFRA_OK;
        }
    }

    infra_printf("Error: Please specify --start, --stop, or --status\n");
    return INFRA_ERROR_INVALID_PARAM;
}

static infra_error_t help_cmd_handler(int argc, char** argv) {
    (void)argc;
    (void)argv;

    INFRA_LOG_DEBUG("Showing help information");

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

    INFRA_LOG_DEBUG("Help information displayed");
    return INFRA_OK;
}

int main(int argc, char** argv) {
    // 初始化命令行框架
    infra_error_t err = poly_cmdline_init();
    if (err != INFRA_OK) {
        printf("Failed to initialize command line framework\n");
        return 1;
    }

    // 初始化memkv命令
    err = poly_memkv_cmd_init();
    if (err != INFRA_OK) {
        printf("Failed to initialize memkv commands\n");
        poly_cmdline_cleanup();
        return 1;
    }

    // 执行命令
    if (argc < 2) {
        poly_cmdline_help(NULL);
        err = INFRA_ERROR_INVALID_PARAM;
    } else {
        err = poly_cmdline_execute(argc - 1, argv + 1);
    }

    // 清理资源
    poly_memkv_cmd_cleanup();
    poly_cmdline_cleanup();

    return (err == INFRA_OK) ? 0 : 1;
} 
