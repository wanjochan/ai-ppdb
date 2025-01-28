#include "../framework/test_framework.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_platform.h"

// 内存分配错误测试
static void test_error_memory(void) {
    // 尝试分配过大的内存
    void* ptr = infra_malloc((size_t)-1);
    TEST_ASSERT(ptr == NULL);
}

// 文件操作错误测试
static void test_error_io(void) {
    infra_error_t err;
    INFRA_CORE_Handle_t handle = 0;
    
    // 尝试打开不存在的文件
    err = infra_file_open("non_existent_file", INFRA_FILE_RDONLY, 0, &handle);
    TEST_ASSERT(err == INFRA_ERROR_IO);
}

// 基本错误测试
static void test_error_basic(void) {
    // 测试错误码转字符串
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_OK), "Success") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_INVALID), "Invalid parameter") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_NO_MEMORY), "No memory") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_TIMEOUT), "Timeout") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_BUSY), "Resource busy") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_NOT_FOUND), "Not found") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_EXISTS), "Already exists") == 0);
    TEST_ASSERT(infra_strcmp(infra_error_string(INFRA_ERROR_IO), "I/O error") == 0);
}

// 边界条件测试
static void test_error_boundary(void) {
    // 测试未知错误码
    TEST_ASSERT(infra_strcmp(infra_error_string(-999), "Unknown error") == 0);
    
    // 测试最大错误码
    TEST_ASSERT(infra_strcmp(infra_error_string(INT32_MAX), "Unknown error") == 0);
}

int main(void) {
    // 初始化infra系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    TEST_BEGIN();
    
    // 运行所有测试
    RUN_TEST(test_error_memory);
    RUN_TEST(test_error_io);
    RUN_TEST(test_error_basic);
    RUN_TEST(test_error_boundary);
    
    TEST_END();
    
    // 清理infra系统
    infra_cleanup();
    return 0;
}
