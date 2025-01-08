#ifndef PPDB_TEST_DATABASE_H
#define PPDB_TEST_DATABASE_H

// 数据库基本功能测试
int test_database_init_destroy(void);
int test_database_transaction_basic(void);
int test_database_transaction_rollback(void);
int test_database_concurrent_transactions(void);
int test_database_data_operations(void);
int test_database_errors(void);
int test_database_boundary_conditions(void);

#endif // PPDB_TEST_DATABASE_H 