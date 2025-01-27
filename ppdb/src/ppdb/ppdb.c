#include "ppdb/ppdb.h"
#include "internal/poly/poly_cmdline.h"
#include "internal/poly/poly_memkv_cmd.h"
#include "internal/infra/infra_core.h"

// 帮助命令处理
static infra_error_t help_cmd_handler(int argc, char** argv) {
    (void)argc;
    (void)argv;

    int cmd_count = 0;
    const poly_cmd_t* commands = poly_cmdline_get_commands(&cmd_count);
    if (commands == NULL) {
        return INFRA_ERROR_NOT_FOUND;
    }

    printf("Usage: ppdb <command> [options]\n\n");
    printf("Available commands:\n");
    for (int i = 0; i < cmd_count; i++) {
        printf("  %-20s %s\n", commands[i].name, commands[i].desc);
        if (commands[i].options && commands[i].option_count > 0) {
            printf("    Options:\n");
            for (int j = 0; j < commands[i].option_count; j++) {
                if (commands[i].options[j].has_value) {
                    printf("      --%s=<value>  %s\n",
                        commands[i].options[j].name,
                        commands[i].options[j].desc);
                } else {
                    printf("      --%s   %s\n",
                        commands[i].options[j].name,
                        commands[i].options[j].desc);
                }
            }
        }
    }

    return INFRA_OK;
}

// 主函数
int main(int argc, char** argv) {
    // 初始化命令行框架
    infra_error_t err = poly_cmdline_init();
    if (err != INFRA_OK) {
        printf("Failed to initialize command line framework\n");
        return 1;
    }

    // 注册帮助命令
    poly_cmd_t help_cmd = {
        .name = "help",
        .desc = "Show help information",
        .options = NULL,
        .option_count = 0,
        .handler = help_cmd_handler
    };
    err = poly_cmdline_register(&help_cmd);
    if (err != INFRA_OK) {
        printf("Failed to register help command\n");
        poly_cmdline_cleanup();
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
        help_cmd_handler(0, NULL);
        err = INFRA_ERROR_INVALID_PARAM;
    } else {
        err = poly_cmdline_execute(argc - 1, argv + 1);
    }

    // 清理资源
    poly_memkv_cmd_cleanup();
    poly_cmdline_cleanup();

    return (err == INFRA_OK) ? 0 : 1;
} 
