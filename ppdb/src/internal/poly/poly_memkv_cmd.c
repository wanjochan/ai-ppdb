#include "internal/poly/poly_memkv.h"
#include "internal/poly/poly_cmdline.h"
#include "internal/poly/poly_plugin.h"
#include "internal/poly/poly_db.h"
#include "internal/infra/infra_error.h"

// 全局插件管理器
static poly_plugin_mgr_t* g_plugin_mgr = NULL;
static poly_plugin_t* g_current_plugin = NULL;

// 在文件开头添加外部声明
extern const poly_builtin_plugin_t g_sqlite_plugin;

// 全局数据库实例
static poly_memkv_db_t* g_db = NULL;

// 帮助信息
static const char* MEMKV_HELP = 
    "memkv - Memory Key-Value Store\n"
    "\n"
    "Usage:\n"
    "  memkv [options] <command> [args...]\n"
    "\n"
    "Options:\n"
    "  --vendor=<n>    Storage vendor (sqlite|duckdb), default: sqlite\n"
    "  --db=<path>       Database path\n"
    "\n"
    "Commands:\n"
    "  get <key>         Get value by key\n"
    "  put <key> <value> Put key-value pair\n"
    "  del <key>         Delete key-value pair\n"
    "  list              List all key-value pairs\n"
    "  help              Show this help message\n";

// 命令行选项
static const poly_cmd_option_t memkv_options[] = {
    {"vendor", "Storage vendor (sqlite|duckdb)", true},
    {"db", "Database path", true}
};

// 获取选项值
static const char* get_option_value(int argc, char** argv, const char* name, const char* default_value) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0) {
            const char* option = argv[i] + 2;
            const char* value = strchr(option, '=');
            if (value) {
                size_t name_len = value - option;
                if (strncmp(option, name, name_len) == 0) {
                    return value + 1;
                }
            }
        }
    }
    return default_value;
}

// 加载存储引擎插件
static infra_error_t load_vendor_plugin(const char* vendor) {
    if (g_current_plugin) {
        poly_plugin_mgr_unload(g_plugin_mgr, g_current_plugin);
        g_current_plugin = NULL;
    }

    if (strcmp(vendor, "sqlite") == 0) {
        // 使用内置SQLite插件
        return poly_plugin_register_builtin(g_plugin_mgr, &g_sqlite_plugin);
    } else if (strcmp(vendor, "duckdb") == 0) {
        const char* plugin_path = "libduckdb.so";//@cosmo_dlopen will auto find .dll/.dylib
        return poly_plugin_mgr_load(g_plugin_mgr,POLY_PLUGIN_DUCKDB,plugin_path,&g_current_plugin);
    }
    
    return INFRA_ERROR_INVALID_PARAM;
}

// 公共操作结构体
typedef struct {
    poly_memkv_db_t* db;
} db_context_t;

// 打开数据库上下文
static infra_error_t open_db_context(const char* vendor, db_context_t* ctx) {
    if (!vendor || !ctx) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建配置
    poly_memkv_config_t config = {
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 1024 * 1024,
        .memory_limit = 0,
        .enable_compression = false,
        .plugin_path = NULL,
        .allow_fallback = true,
        .read_only = false
    };

    // 创建数据库实例
    return poly_memkv_create(&config, &ctx->db);
}

// 关闭数据库上下文
static void close_db_context(db_context_t* ctx) {
    if (ctx) {
        if (ctx->db) {
            poly_memkv_destroy(ctx->db);
            ctx->db = NULL;
        }
    }
}

// 命令处理函数
static infra_error_t cmd_get(int argc, char** argv) {
    if (argc < 2) return INFRA_ERROR_INVALID_PARAM;
    
    const char* vendor = get_option_value(argc, argv, "vendor", "sqlite");
    const char* db_path = get_option_value(argc, argv, "db", "memkv.db");
    const char* key = argv[1];

    db_context_t ctx = {0};
    infra_error_t err = open_db_context(vendor, &ctx);
    if (err != INFRA_OK) return err;

    void* value = NULL;
    size_t value_len = 0;
    err = poly_memkv_get(ctx.db, key, &value, &value_len);

    if (err == INFRA_OK) {
        printf("%.*s\n", (int)value_len, (char*)value);
        infra_free(value);
    } else if (err == POLY_MEMKV_ERROR_KEY_NOT_FOUND) {
        printf("Key not found: %s\n", key);
    }

    close_db_context(&ctx);
    return err;
}

static infra_error_t cmd_put(int argc, char** argv) {
    if (argc < 3) return INFRA_ERROR_INVALID_PARAM;

    const char* vendor = get_option_value(argc, argv, "vendor", "sqlite");
    const char* db_path = get_option_value(argc, argv, "db", "memkv.db");
    const char* key = argv[1];
    const char* value = argv[2];

    db_context_t ctx = {0};
    infra_error_t err = open_db_context(vendor, &ctx);
    if (err != INFRA_OK) return err;

    err = poly_memkv_set(ctx.db, key, value, strlen(value));
    close_db_context(&ctx);
    return err;
}

static infra_error_t cmd_del(int argc, char** argv) {
    if (argc < 2) return INFRA_ERROR_INVALID_PARAM;

    const char* vendor = get_option_value(argc, argv, "vendor", "sqlite");
    const char* db_path = get_option_value(argc, argv, "db", "memkv.db");
    const char* key = argv[1];

    db_context_t ctx = {0};
    infra_error_t err = open_db_context(vendor, &ctx);
    if (err != INFRA_OK) return err;

    err = poly_memkv_del(ctx.db, key);
    close_db_context(&ctx);
    return err;
}

static infra_error_t cmd_list(int argc, char** argv) {
    const char* vendor = get_option_value(argc, argv, "vendor", "sqlite");
    const char* db_path = get_option_value(argc, argv, "db", "memkv.db");

    db_context_t ctx = {0};
    infra_error_t err = open_db_context(vendor, &ctx);
    if (err != INFRA_OK) return err;

    poly_memkv_iter_t* iter = NULL;
    err = poly_memkv_iter_create(ctx.db, &iter);
    if (err != INFRA_OK) {
        close_db_context(&ctx);
        return err;
    }

    char* key = NULL;
    void* value = NULL;
    size_t value_len = 0;

    while (1) {
        err = poly_memkv_iter_next(iter, &key, &value, &value_len);
        if (err == POLY_MEMKV_ERROR_KEY_NOT_FOUND) {
            err = INFRA_OK;
            break;
        }
        if (err != INFRA_OK) break;

        printf("%s: %.*s\n", key, (int)value_len, (char*)value);
        infra_free(key);
        infra_free(value);
        key = NULL;
        value = NULL;
    }

    if (key) infra_free(key);
    if (value) infra_free(value);
    poly_memkv_iter_destroy(iter);
    close_db_context(&ctx);
    return err;
}

static infra_error_t cmd_help(int argc, char** argv) {
    printf("%s", MEMKV_HELP);
    return INFRA_OK;
}

// 命令定义
static const poly_cmd_t memkv_commands[] = {
    {
        .name = "get",
        .desc = "Get value by key",
        .options = memkv_options,
        .option_count = sizeof(memkv_options) / sizeof(memkv_options[0]),
        .handler = cmd_get
    },
    {
        .name = "put",
        .desc = "Put key-value pair",
        .options = memkv_options,
        .option_count = sizeof(memkv_options) / sizeof(memkv_options[0]),
        .handler = cmd_put
    },
    {
        .name = "del",
        .desc = "Delete key-value pair",
        .options = memkv_options,
        .option_count = sizeof(memkv_options) / sizeof(memkv_options[0]),
        .handler = cmd_del
    },
    {
        .name = "list",
        .desc = "List all key-value pairs",
        .options = memkv_options,
        .option_count = sizeof(memkv_options) / sizeof(memkv_options[0]),
        .handler = cmd_list
    },
    {
        .name = "help",
        .desc = "Show help message",
        .options = NULL,
        .option_count = 0,
        .handler = cmd_help
    }
};

// 初始化memkv命令行
infra_error_t poly_memkv_cmd_init(void) {
    return INFRA_OK;
}

void poly_memkv_cmd_cleanup(void) {
    if (g_db) {
        poly_memkv_destroy(g_db);
        g_db = NULL;
    }
}

// 处理memkv命令
infra_error_t poly_memkv_cmd_process(int argc, char* argv[]) {
    if (argc < 2) {
        return cmd_help(argc, argv);
    }

    const char* cmd = argv[1];
    for (size_t i = 0; i < sizeof(memkv_commands) / sizeof(memkv_commands[0]); i++) {
        if (strcmp(cmd, memkv_commands[i].name) == 0) {
            return memkv_commands[i].handler(argc - 1, argv + 1);
        }
    }

    printf("Unknown command: %s\n", cmd);
    return cmd_help(argc, argv);
} 
