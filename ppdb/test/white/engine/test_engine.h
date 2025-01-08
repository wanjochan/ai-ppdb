#ifndef PPDB_TEST_ENGINE_H
#define PPDB_TEST_ENGINE_H

// 基础测试
int test_engine_init_destroy(void);
int test_engine_transaction_basic(void);

// 事务测试
int test_engine_transaction_rollback(void);
int test_engine_concurrent_transactions(void);

// 数据操作测试
int test_engine_data_operations(void);

// 错误处理测试
int test_engine_errors(void);

// 边界条件测试
int test_engine_boundary_conditions(void);

#endif // PPDB_TEST_ENGINE_H 