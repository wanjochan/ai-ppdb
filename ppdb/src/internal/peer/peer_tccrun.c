#include "internal/peer/peer_tccrun.h"
#include "internal/infra/infra_core.h"
#include "internal/poly/poly_cmdline.h"

// TinyCC 头文件
#include "repos/tinycc/libtcc.h"

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

static tccrun_context_t g_context = {0};
static infra_mutex_t g_mutex = NULL;

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

const poly_cmd_option_t tccrun_options[] = {
    {"source", "Source file path", true},
    {"args", "Program arguments", false},
};

const int tccrun_option_count = sizeof(tccrun_options) / sizeof(tccrun_options[0]);

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static void tcc_error_func(void* opaque, const char* msg) {
    INFRA_LOG_ERROR("TCC Error: %s", msg);
}

//-----------------------------------------------------------------------------
// Core Functions Implementation
//-----------------------------------------------------------------------------

infra_error_t tccrun_init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_mutex != NULL) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // 清空上下文
    memset(&g_context, 0, sizeof(g_context));

    // 创建互斥锁
    infra_error_t err = infra_mutex_create(&g_mutex);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create mutex: %d", err);
        return err;
    }

    INFRA_LOG_INFO("TCC run service initialized successfully");
    return INFRA_OK;
}

infra_error_t tccrun_cleanup(void) {
    if (g_mutex != NULL) {
        infra_mutex_destroy(g_mutex);
        g_mutex = NULL;
    }

    memset(&g_context, 0, sizeof(g_context));
    return INFRA_OK;
}

infra_error_t tccrun_execute(const char* source_path, int argc, char** argv) {
    if (!source_path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建 TCC 状态
    TCCState* s = tcc_new();
    if (!s) {
        INFRA_LOG_ERROR("Could not create TCC state");
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置错误处理函数
    tcc_set_error_func(s, NULL, tcc_error_func);

    // 设置输出类型为内存
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    // 编译源文件
    if (tcc_add_file(s, source_path) == -1) {
        INFRA_LOG_ERROR("Could not compile '%s'", source_path);
        tcc_delete(s);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 重定位代码
    if (tcc_relocate(s, TCC_RELOCATE_AUTO) < 0) {
        INFRA_LOG_ERROR("Could not relocate code");
        tcc_delete(s);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 获取入口点
    int (*func)(int, char**) = (int (*)(int, char**))tcc_get_symbol(s, "main");
    if (!func) {
        INFRA_LOG_ERROR("Could not find main() function");
        tcc_delete(s);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 运行程序
    int ret = func(argc, argv);
    INFRA_LOG_INFO("Program exited with code: %d", ret);

    // 清理
    tcc_delete(s);
    return INFRA_OK;
}

infra_error_t tccrun_cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        INFRA_LOG_ERROR("Invalid arguments");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化服务
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_error_t err = tccrun_init(&config);
    if (err != INFRA_OK && err != INFRA_ERROR_ALREADY_EXISTS) {
        INFRA_LOG_ERROR("Failed to initialize tccrun service");
        return err;
    }

    // 处理命令行参数
    const char* source_path = NULL;
    int prog_argc = 0;
    char* prog_argv[TCCRUN_MAX_ARGS] = {0};

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "--source") == 0) {
            if (i + 1 >= argc) {
                INFRA_LOG_ERROR("Missing source file path");
                return INFRA_ERROR_INVALID_PARAM;
            }
            source_path = argv[++i];
        } else if (strcmp(arg, "--args") == 0) {
            // 收集程序参数
            prog_argv[prog_argc++] = (char*)source_path;  // argv[0] 是程序名
            while (++i < argc && prog_argc < TCCRUN_MAX_ARGS) {
                prog_argv[prog_argc++] = argv[i];
            }
            break;
        }
    }

    if (!source_path) {
        INFRA_LOG_ERROR("No source file specified");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 如果没有提供参数，至少传入程序名
    if (prog_argc == 0) {
        prog_argv[prog_argc++] = (char*)source_path;
    }

    // 编译并运行程序
    return tccrun_execute(source_path, prog_argc, prog_argv);
} 