#include "internal/poly/poly_memkv.h"
#include "internal/poly/poly_cmdline.h"
#include "internal/poly/poly_plugin.h"
#include "internal/poly/poly_sqlite.h"
#include "internal/poly/poly_duckdb.h"

// 全局插件管理器
static poly_plugin_mgr_t* g_plugin_mgr = NULL;
static poly_plugin_t* g_current_plugin = NULL;

// 在文件开头添加外部声明
extern const poly_builtin_plugin_t g_sqlite_plugin;

// 帮助信息
static const char* MEMKV_HELP = 
    "memkv - Memory Key-Value Store\n"
    "\n"
    "Usage:\n"
    "  memkv [options] <command> [args...]\n"
    "\n"
    "Options:\n"
    "  --vendor=<name>    Storage vendor (sqlite|duckdb), default: sqlite\n"
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

// 命令处理函数
static infra_error_t cmd_get(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: get <key>\n");
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* vendor = get_option_value(argc, argv, "vendor", "sqlite");
    const char* db_path = get_option_value(argc, argv, "db", "memkv.db");
    const char* key = argv[1];

    // 加载插件
    infra_error_t err = load_vendor_plugin(vendor);
    if (err != INFRA_OK) return err;

    // 打开数据库
    void* db;
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        err = sqlite->open(db_path, (poly_sqlitekv_db_t**)&db);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        err = duckdb->open(db_path, (poly_duckdbkv_db_t**)&db);
    }
    if (err != INFRA_OK) return err;

    // 获取数据
    void* value;
    size_t value_len;
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        err = sqlite->get(db, key, strlen(key), &value, &value_len);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        err = duckdb->get(db, key, strlen(key), &value, &value_len);
    }

    if (err == INFRA_OK) {
        printf("%.*s\n", (int)value_len, (char*)value);
        infra_free(value);
    } else if (err == INFRA_ERROR_NOT_FOUND) {
        printf("Key not found: %s\n", key);
    }

    // 关闭数据库
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        sqlite->close(db);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        duckdb->close(db);
    }

    return err;
}

static infra_error_t cmd_put(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: put <key> <value>\n");
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* vendor = get_option_value(argc, argv, "vendor", "sqlite");
    const char* db_path = get_option_value(argc, argv, "db", "memkv.db");
    const char* key = argv[1];
    const char* value = argv[2];

    // 加载插件
    infra_error_t err = load_vendor_plugin(vendor);
    if (err != INFRA_OK) return err;

    // 打开数据库
    void* db;
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        err = sqlite->open(db_path, (poly_sqlitekv_db_t**)&db);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        err = duckdb->open(db_path, (poly_duckdbkv_db_t**)&db);
    }
    if (err != INFRA_OK) return err;

    // 存储数据
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        err = sqlite->put(db, key, strlen(key), value, strlen(value));
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        err = duckdb->put(db, key, strlen(key), value, strlen(value));
    }

    // 关闭数据库
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        sqlite->close(db);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        duckdb->close(db);
    }

    return err;
}

static infra_error_t cmd_del(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: del <key>\n");
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* vendor = get_option_value(argc, argv, "vendor", "sqlite");
    const char* db_path = get_option_value(argc, argv, "db", "memkv.db");
    const char* key = argv[1];

    // 加载插件
    infra_error_t err = load_vendor_plugin(vendor);
    if (err != INFRA_OK) return err;

    // 打开数据库
    void* db;
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        err = sqlite->open(db_path, (poly_sqlitekv_db_t**)&db);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        err = duckdb->open(db_path, (poly_duckdbkv_db_t**)&db);
    }
    if (err != INFRA_OK) return err;

    // 删除数据
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        err = sqlite->del(db, key, strlen(key));
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        err = duckdb->del(db, key, strlen(key));
    }

    // 关闭数据库
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        sqlite->close(db);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        duckdb->close(db);
    }

    return err;
}

static infra_error_t cmd_list(int argc, char** argv) {
    const char* vendor = get_option_value(argc, argv, "vendor", "sqlite");
    const char* db_path = get_option_value(argc, argv, "db", "memkv.db");

    // 加载插件
    infra_error_t err = load_vendor_plugin(vendor);
    if (err != INFRA_OK) return err;

    // 打开数据库
    void* db;
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        err = sqlite->open(db_path, (poly_sqlitekv_db_t**)&db);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        err = duckdb->open(db_path, (poly_duckdbkv_db_t**)&db);
    }
    if (err != INFRA_OK) return err;

    // 创建迭代器
    void* iter;
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        err = sqlite->iter_create(db, (poly_sqlitekv_iter_t**)&iter);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        err = duckdb->iter_create(db, (poly_duckdbkv_iter_t**)&iter);
    }
    if (err != INFRA_OK) goto cleanup;

    // 遍历数据
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;

    while (1) {
        if (strcmp(vendor, "sqlite") == 0) {
            sqlite_interface_t* sqlite = g_current_plugin->interface;
            err = sqlite->iter_next(iter, &key, &key_len, &value, &value_len);
        } else {
            duckdb_interface_t* duckdb = g_current_plugin->interface;
            err = duckdb->iter_next(iter, &key, &key_len, &value, &value_len);
        }

        if (err == INFRA_ERROR_NOT_FOUND) {
            break;
        } else if (err != INFRA_OK) {
            goto cleanup;
        }

        printf("%.*s: %.*s\n", (int)key_len, (char*)key, (int)value_len, (char*)value);
        infra_free(key);
        infra_free(value);
    }

cleanup:
    // 销毁迭代器
    if (strcmp(vendor, "sqlite") == 0) {
        sqlite_interface_t* sqlite = g_current_plugin->interface;
        sqlite->iter_destroy(iter);
        sqlite->close(db);
    } else {
        duckdb_interface_t* duckdb = g_current_plugin->interface;
        duckdb->iter_destroy(iter);
        duckdb->close(db);
    }

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
    // 创建插件管理器
    infra_error_t err = poly_plugin_mgr_create(&g_plugin_mgr);
    if (err != INFRA_OK) return err;

    // 注册命令
    for (size_t i = 0; i < sizeof(memkv_commands) / sizeof(memkv_commands[0]); i++) {
        err = poly_cmdline_register(&memkv_commands[i]);
        if (err != INFRA_OK) {
            poly_plugin_mgr_destroy(g_plugin_mgr);
            return err;
        }
    }

    return INFRA_OK;
}

// 清理memkv命令行
infra_error_t poly_memkv_cmd_cleanup(void) {
    if (g_current_plugin) {
        poly_plugin_mgr_unload(g_plugin_mgr, g_current_plugin);
        g_current_plugin = NULL;
    }

    if (g_plugin_mgr) {
        poly_plugin_mgr_destroy(g_plugin_mgr);
        g_plugin_mgr = NULL;
    }

    return INFRA_OK;
} 
