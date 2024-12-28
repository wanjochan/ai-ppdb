#include <cosmopolitan.h>
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/logger.h"
#include "ppdb/kvstore.h"

// 打印帮助信息
static void print_usage(void) {
    printf("PPDB - 高性能键值存储引擎\n");
    printf("\n用法:\n");
    printf("  ppdb [选项] <命令> [参数...]\n");
    printf("\n选项:\n");
    printf("  --mode <locked|lockfree>  运行模式 (默认: locked)\n");
    printf("  --dir <path>             数据目录路径 (默认: db)\n");
    printf("  --memtable-size <bytes>  内存表大小 (默认: 1MB)\n");
    printf("  --l0-size <bytes>        L0层文件大小 (默认: 1MB)\n");
    printf("  --help                   显示此帮助信息\n");
    printf("\n命令:\n");
    printf("  put <key> <value>        存储键值对\n");
    printf("  get <key>                获取键对应的值\n");
    printf("  delete <key>             删除键值对\n");
    printf("  list                     列出所有键值对\n");
    printf("  stats                    显示数据库统计信息\n");
    printf("  server                   启动 HTTP API 服务器\n");
    printf("\n示例:\n");
    printf("  ppdb --mode lockfree put mykey myvalue\n");
    printf("  ppdb get mykey\n");
    printf("  ppdb --dir /path/to/db server\n");
}

// 解析命令行参数
typedef struct {
    ppdb_mode_t mode;
    char dir_path[256];
    size_t memtable_size;
    size_t l0_size;
    char command[32];
    char key[256];
    char value[1024];
} cli_options_t;

static void parse_options(int argc, char* argv[], cli_options_t* opts) {
    // 设置默认值
    opts->mode = PPDB_MODE_LOCKED;
    strncpy(opts->dir_path, "db", sizeof(opts->dir_path) - 1);
    opts->memtable_size = 1024 * 1024;  // 1MB
    opts->l0_size = 1024 * 1024;        // 1MB
    memset(opts->command, 0, sizeof(opts->command));
    memset(opts->key, 0, sizeof(opts->key));
    memset(opts->value, 0, sizeof(opts->value));

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            exit(0);
        }
        else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "lockfree") == 0) {
                opts->mode = PPDB_MODE_LOCKFREE;
            }
            i++;
        }
        else if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            strncpy(opts->dir_path, argv[i + 1], sizeof(opts->dir_path) - 1);
            i++;
        }
        else if (strcmp(argv[i], "--memtable-size") == 0 && i + 1 < argc) {
            opts->memtable_size = atol(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--l0-size") == 0 && i + 1 < argc) {
            opts->l0_size = atol(argv[i + 1]);
            i++;
        }
        else if (opts->command[0] == '\0') {
            strncpy(opts->command, argv[i], sizeof(opts->command) - 1);
        }
        else if (opts->key[0] == '\0') {
            strncpy(opts->key, argv[i], sizeof(opts->key) - 1);
        }
        else if (opts->value[0] == '\0') {
            strncpy(opts->value, argv[i], sizeof(opts->value) - 1);
        }
    }
}

// 执行命令
static int execute_command(ppdb_kvstore_t* store, const cli_options_t* opts) {
    ppdb_error_t err;
    
    if (strcmp(opts->command, "put") == 0) {
        if (opts->key[0] == '\0' || opts->value[0] == '\0') {
            printf("错误: put 命令需要 key 和 value 参数\n");
            return 1;
        }
        err = ppdb_kvstore_put(store, 
                              (const uint8_t*)opts->key, strlen(opts->key),
                              (const uint8_t*)opts->value, strlen(opts->value));
        if (err != PPDB_OK) {
            printf("错误: 存储键值对失败: %s\n", ppdb_error_string(err));
            return 1;
        }
        printf("成功: 已存储键值对\n");
    }
    else if (strcmp(opts->command, "get") == 0) {
        if (opts->key[0] == '\0') {
            printf("错误: get 命令需要 key 参数\n");
            return 1;
        }
        uint8_t* value = NULL;
        size_t value_size = 0;
        err = ppdb_kvstore_get(store, 
                              (const uint8_t*)opts->key, strlen(opts->key),
                              &value, &value_size);
        if (err == PPDB_ERR_NOT_FOUND) {
            printf("未找到键: %s\n", opts->key);
            return 1;
        }
        if (err != PPDB_OK) {
            printf("错误: 获取值失败: %s\n", ppdb_error_string(err));
            return 1;
        }
        printf("%.*s\n", (int)value_size, value);
        free(value);
    }
    else if (strcmp(opts->command, "delete") == 0) {
        if (opts->key[0] == '\0') {
            printf("错误: delete 命令需要 key 参数\n");
            return 1;
        }
        err = ppdb_kvstore_delete(store, 
                                 (const uint8_t*)opts->key, strlen(opts->key));
        if (err != PPDB_OK) {
            printf("错误: 删除键值对失败: %s\n", ppdb_error_string(err));
            return 1;
        }
        printf("成功: 已删除键值对\n");
    }
    else if (strcmp(opts->command, "stats") == 0) {
        printf("数据库统计信息:\n");
        printf("- 运行模式: %s\n", opts->mode == PPDB_MODE_LOCKFREE ? "无锁" : "加锁");
        printf("- 数据目录: %s\n", opts->dir_path);
        printf("- MemTable 大小限制: %zu bytes\n", opts->memtable_size);
        printf("- L0 文件大小限制: %zu bytes\n", opts->l0_size);
        // TODO: 添加更多统计信息
    }
    else if (strcmp(opts->command, "server") == 0) {
        printf("HTTP API 服务器功能尚未实现\n");
        return 1;
    }
    else {
        printf("错误: 未知命令: %s\n", opts->command);
        print_usage();
        return 1;
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    // 如果没有参数，显示帮助信息
    if (argc == 1) {
        print_usage();
        return 0;
    }

    // 解析命令行参数
    cli_options_t opts;
    parse_options(argc, argv, &opts);

    // 如果没有指定命令，显示错误
    if (opts.command[0] == '\0') {
        printf("错误: 未指定命令\n");
        print_usage();
        return 1;
    }

    ppdb_log_info("PPDB starting...");
    ppdb_log_info("Running in %s mode", opts.mode == PPDB_MODE_LOCKFREE ? "lock-free" : "locked");

    // 创建 KVStore 配置
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = opts.memtable_size,
        .l0_size = opts.l0_size,
        .l0_files = 4,
        .compression = PPDB_COMPRESSION_NONE,
        .mode = opts.mode
    };
    strncpy(config.dir_path, opts.dir_path, sizeof(config.dir_path) - 1);

    // 创建 KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create KVStore: %s", ppdb_error_string(err));
        return 1;
    }

    ppdb_log_info("PPDB started successfully");

    // 执行命令
    int ret = execute_command(store, &opts);

    // 清理资源
    ppdb_kvstore_destroy(store);

    return ret;
}