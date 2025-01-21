#include "internal/peer/peer_tccrun.h"
#include "internal/infra/infra_core.h"
#include "internal/poly/poly_cmdline.h"
#include "internal/poly/poly_tcc.h"
#include "cosmopolitan.h"  // TODO cosmo/infra later: 需要文件操作函数

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
    {"I", "Add include path", false},
    {"L", "Add library path", false},
    {"args", "Program arguments", false},
};

const int tccrun_option_count = sizeof(tccrun_options) / sizeof(tccrun_options[0]);

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
    infra_memset(&g_context, 0, sizeof(g_context));

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

    infra_memset(&g_context, 0, sizeof(g_context));
    return INFRA_OK;
}

infra_error_t tccrun_execute(const char* source_path, int argc, char** argv) {
    if (!source_path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建 TCC 状态
    poly_tcc_state_t* s = poly_tcc_new();
    if (!s) {
        INFRA_LOG_ERROR("Could not create TCC state");
        return INFRA_ERROR_NO_MEMORY;
    }

    // TODO cosmo/infra later: 使用 infra 层的文件操作函数
    FILE* fp = fopen(source_path, "rb");
    if (!fp) {
        INFRA_LOG_ERROR("Could not open '%s'", source_path);
        poly_tcc_delete(s);
        return INFRA_ERROR_NOT_FOUND;
    }

    // TODO cosmo/infra later: 使用 infra 层的文件操作函数
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 读取文件内容
    char* source = poly_tcc_malloc(size + 1);
    if (!source) {
        INFRA_LOG_ERROR("Could not allocate memory for source");
        fclose(fp);  // TODO cosmo/infra later: 使用 infra 层的文件操作函数
        poly_tcc_delete(s);
        return INFRA_ERROR_NO_MEMORY;
    }

    // TODO cosmo/infra later: 使用 infra 层的文件操作函数
    if (fread(source, 1, size, fp) != (size_t)size) {
        INFRA_LOG_ERROR("Could not read source file");
        fclose(fp);
        poly_tcc_free(source);
        poly_tcc_delete(s);
        return INFRA_ERROR_IO;
    }
    source[size] = '\0';
    fclose(fp);  // TODO cosmo/infra later: 使用 infra 层的文件操作函数

    // 编译源代码
    if (poly_tcc_compile_string(s, source) != 0) {
        INFRA_LOG_ERROR("Could not compile source: %s", poly_tcc_get_error_msg(s));
        poly_tcc_free(source);
        poly_tcc_delete(s);
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_tcc_free(source);

    // 执行代码
    int ret = poly_tcc_run(s, argc, argv);
    
    // 清理
    poly_tcc_delete(s);

    return ret == 0 ? INFRA_OK : INFRA_ERROR_RUNTIME;
}

infra_error_t tccrun_cmd_handler(int argc, char** argv) {
    INFRA_LOG_DEBUG("tccrun_cmd_handler: argc=%d", argc);
    for (int i = 0; i < argc; i++) {
        INFRA_LOG_DEBUG("  argv[%d] = %s", i, argv[i]);
    }

    if (argc < 2) {
        INFRA_LOG_ERROR("Invalid arguments");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 处理命令行参数
    const char* source_path = NULL;
    int prog_argc = 0;
    char* prog_argv[TCCRUN_MAX_ARGS] = {0};

    // 创建 TCC 状态
    poly_tcc_state_t* s = poly_tcc_new();
    if (!s) {
        INFRA_LOG_ERROR("Could not create TCC state");
        return INFRA_ERROR_NO_MEMORY;
    }

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        INFRA_LOG_DEBUG("Processing arg[%d]: %s", i, arg);
        if (strcmp(arg, "--source") == 0) {
            if (i + 1 >= argc) {
                INFRA_LOG_ERROR("Missing source file path");
                poly_tcc_delete(s);
                return INFRA_ERROR_INVALID_PARAM;
            }
            source_path = argv[++i];
            INFRA_LOG_DEBUG("Found source path: %s", source_path);
        } else if (strcmp(arg, "--I") == 0) {
            if (i + 1 >= argc) {
                INFRA_LOG_ERROR("Missing include path");
                poly_tcc_delete(s);
                return INFRA_ERROR_INVALID_PARAM;
            }
            poly_tcc_add_include_path(s, argv[++i]);
            INFRA_LOG_DEBUG("Added include path: %s", argv[i]);
        } else if (strcmp(arg, "--L") == 0) {
            if (i + 1 >= argc) {
                INFRA_LOG_ERROR("Missing library path");
                poly_tcc_delete(s);
                return INFRA_ERROR_INVALID_PARAM;
            }
            poly_tcc_add_library_path(s, argv[++i]);
            INFRA_LOG_DEBUG("Added library path: %s", argv[i]);
        } else if (strcmp(arg, "--args") == 0) {
            // 收集程序参数
            prog_argv[prog_argc++] = (char*)source_path;  // argv[0] 是程序名
            INFRA_LOG_DEBUG("Added program name: %s", source_path);
            while (++i < argc && prog_argc < TCCRUN_MAX_ARGS) {
                prog_argv[prog_argc++] = argv[i];
                INFRA_LOG_DEBUG("Added program arg: %s", argv[i]);
            }
            break;
        }
    }

    if (!source_path) {
        INFRA_LOG_ERROR("No source file specified");
        poly_tcc_delete(s);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 如果没有提供参数，至少传入程序名
    if (prog_argc == 0) {
        prog_argv[prog_argc++] = (char*)source_path;
        INFRA_LOG_DEBUG("Using source path as program name: %s", source_path);
    }

    INFRA_LOG_DEBUG("Opening source file: %s", source_path);

    // 读取源文件
    FILE* fp = fopen(source_path, "rb");  // TODO cosmo/infra later: 使用 infra 文件操作
    if (!fp) {
        INFRA_LOG_ERROR("Could not open '%s': %s", source_path, strerror(errno));
        poly_tcc_delete(s);
        return INFRA_ERROR_NOT_FOUND;
    }

    // 获取文件大小
    fseek(fp, 0, SEEK_END);  // TODO cosmo/infra later: 使用 infra 文件操作
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // 读取文件内容
    char* source = poly_tcc_malloc(size + 1);
    if (!source) {
        INFRA_LOG_ERROR("Could not allocate memory for source");
        fclose(fp);  // TODO cosmo/infra later: 使用 infra 文件操作
        poly_tcc_delete(s);
        return INFRA_ERROR_NO_MEMORY;
    }

    if (fread(source, 1, size, fp) != (size_t)size) {  // TODO cosmo/infra later: 使用 infra 文件操作
        INFRA_LOG_ERROR("Could not read source file");
        fclose(fp);
        poly_tcc_free(source);
        poly_tcc_delete(s);
        return INFRA_ERROR_IO;
    }
    source[size] = '\0';
    fclose(fp);  // TODO cosmo/infra later: 使用 infra 文件操作

    // 编译源代码
    if (poly_tcc_compile_string(s, source) != 0) {
        INFRA_LOG_ERROR("Could not compile source: %s", poly_tcc_get_error_msg(s));
        poly_tcc_free(source);
        poly_tcc_delete(s);
        return INFRA_ERROR_INVALID_PARAM;
    }

    poly_tcc_free(source);

    // 执行代码
    int ret = poly_tcc_run(s, prog_argc, prog_argv);
    
    // 清理
    poly_tcc_delete(s);

    INFRA_LOG_INFO("Program execution completed with return value: %d", ret);
    return INFRA_OK;  // Always return OK since the program executed successfully
} 