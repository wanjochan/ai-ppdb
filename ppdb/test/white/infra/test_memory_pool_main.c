/*
 * test_memory_pool_main.c - Memory Pool Test Suite Main Program
 */

#include "test/white/infra/test_memory_pool.h"
#include "cosmopolitan.h"

int main(void) {
    // 初始化随机数生成器，用于随机测试
    srand((unsigned int)time(NULL));
    
    // 运行测试套件
    return run_memory_pool_test_suite();
} 