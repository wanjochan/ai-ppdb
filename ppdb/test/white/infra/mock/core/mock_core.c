#include "test/white/framework/test_framework.h"
#include "test/white/framework/mock_framework.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"

// 只保留需要 mock 的日志相关函数
static infra_log_callback_t g_log_callback = NULL;
static int g_log_level = INFRA_LOG_LEVEL_INFO;

__attribute__((weak)) void infra_log_set_callback(infra_log_callback_t callback) {
    mock_function_call("infra_log_set_callback");
    mock_param_ptr("callback", callback);
    g_log_callback = callback;
}

__attribute__((weak)) void infra_log_set_level(int level) {
    mock_function_call("infra_log_set_level");
    mock_param_value("level", level);
    if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
        g_log_level = level;
    }
}

// 只 mock 需要特殊控制的内存分配函数
__attribute__((weak)) void* infra_malloc(size_t size) {
    mock_function_call("infra_malloc");
    mock_param_value("size", size);
    void* ptr = mock_return_ptr("infra_malloc");
    if (!ptr) {
        errno = ENOMEM;
    }
    return ptr;
}

// 文件操作需要 mock 以模拟错误情况
__attribute__((weak)) infra_error_t infra_file_open(const char* path, infra_flags_t flags, int mode, INFRA_CORE_Handle_t* handle) {
    mock_function_call("infra_file_open");
    mock_param_str("path", path);
    mock_param_value("flags", flags);
    mock_param_value("mode", mode);
    mock_param_ptr("handle", handle);
    
    infra_error_t err = mock_return_value("infra_file_open");
    if (err == INFRA_OK && handle) {
        *handle = 1;
    }
    return err;
}

// 日志记录需要 mock 以验证日志行为
void mock_log(int level, const char* file, int line, const char* func, const char* format, ...) {
    mock_function_call("mock_log");
    mock_param_value("level", level);
    mock_param_str("file", file);
    mock_param_value("line", line);
    mock_param_str("func", func);
    mock_param_str("format", format);
    
    if (level > g_log_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    mock_param_str("message", message);

    if (g_log_callback) {
        g_log_callback(level, file, line, func, message);
    }
} 