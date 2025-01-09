#ifndef TEST_MEMKV_H_
#define TEST_MEMKV_H_

#include "cosmopolitan.h"
#include "../test_framework.h"

// 测试用例声明
int test_memkv_create(void);
int test_memkv_basic_ops(void);
int test_memkv_batch_ops(void);
int test_memkv_iter_ops(void);
int test_memkv_snapshot(void);
int test_memkv_status(void);

// 运行所有测试
int run_memkv_tests(void);

#endif // TEST_MEMKV_H_
