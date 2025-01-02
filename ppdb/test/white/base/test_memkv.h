#ifndef TEST_MEMKV_H_
#define TEST_MEMKV_H_

#include "ppdb/ppdb_memkv.h"
#include "test/white/test_framework.h"

// 测试用例声明
void test_memkv_create(void);
void test_memkv_basic_ops(void);
void test_memkv_batch_ops(void);
void test_memkv_iter_ops(void);
void test_memkv_snapshot(void);
void test_memkv_status(void);

// 运行所有测试
void run_memkv_tests(void);

#endif // TEST_MEMKV_H_
