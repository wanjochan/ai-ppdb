#include "test_common.h"
#include "internal/infra/infra.h"
#include "test_framework.h"

// 基本错误测试
static int test_error_basic(void) {
    // 测试错误码转字符串
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_OK), "Success") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_INVALID), "Invalid parameter") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_MEMORY), "Memory error") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_TIMEOUT), "Timeout") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_BUSY), "Resource busy") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_NOT_FOUND), "Not found") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_EXISTS), "Already exists") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_IO), "I/O error") == 0);
    
    return 0;
}

// 错误传播测试
static int test_error_propagation(void) {
    infra_error_t err;
    
    // 测试内存分配失败的错误传播
    void* ptr = infra_malloc((size_t)-1);  // 尝试分配过大的内存
    TEST_ASSERT(ptr == NULL);
    
    // 测试文件操作错误传播
    infra_handle_t handle;
    err = infra_file_open("non_existent_file", INFRA_FILE_RDONLY, 0, &handle);
    TEST_ASSERT(err == INFRA_ERROR_IO);
    
    return 0;
}

// 边界条件测试
static int test_error_boundary(void) {
    // 测试未知错误码
    TEST_ASSERT(infra_strcmp(infra_error_string(-999), "Unknown error") == 0);
    
    // 测试最大错误码
    TEST_ASSERT(infra_strcmp(infra_error_string(INT32_MAX), "Unknown error") == 0);
    
    return 0;
}

// 错误状态测试
static int test_error_status(void) {
    infra_status_t status;
    infra_error_t err;
    
    // 获取状态
    err = infra_get_status(&status);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(status.initialized == true);
    TEST_ASSERT((status.active_flags & INFRA_INIT_ALL) != 0);
    
    // 测试空指针
    err = infra_get_status(NULL);
    TEST_ASSERT(err == INFRA_ERROR_INVALID);
    
    return 0;
}

int main(void) {
    // 初始化infra系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    TEST_INIT();
    
    TEST_RUN(test_error_basic);
    TEST_RUN(test_error_propagation);
    TEST_RUN(test_error_boundary);
    TEST_RUN(test_error_status);
    
    TEST_CLEANUP();
    
    // 清理infra系统
    infra_cleanup();
    return 0;
}
