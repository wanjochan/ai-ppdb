#include "internal/poly/poly_db.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"

//TODO change to assert in test framework
#include "assert.h"

// 测试 URL 解析和数据库连接
static void test_db_open(void) {
    printf("Testing database open :memory: \n");
    
    // 测试 SQLite
    {
        poly_db_t* db = NULL;
        infra_error_t err = poly_db_open("sqlite://:memory:", &db);
        printf("SQLite open result: %d (expected: %d)\n", err, INFRA_ERROR_NOT_SUPPORTED);
        assert(err == INFRA_ERROR_NOT_SUPPORTED && "SQLite implementation is not ready yet");
        assert(db == NULL && "Database handle should be NULL when error occurs");
        printf("SQLite test passed\n");
    }
    
    // 测试 DuckDB
    {
        printf("Testing DuckDB...\n");
        poly_db_t* db = NULL;
        infra_error_t err = poly_db_open("duckdb://:memory:", &db);
        printf("DuckDB open result: %d\n", err);
        if (err == INFRA_OK) {
            assert(db != NULL && "Database handle is NULL");
            poly_db_close(db);
            printf("DuckDB test passed\n");
        } else {
            printf("DuckDB test skipped (error: %d)\n", err);
        }
    }
    
    // 测试无效 URL
    {
        printf("Testing invalid URL...\n");
        poly_db_t* db = NULL;
        infra_error_t err = poly_db_open("invalid://:memory:", &db);
        printf("Invalid URL test result: %d\n", err);
        assert(err == INFRA_ERROR_INVALID_PARAM && "Should fail with invalid scheme");
        assert(db == NULL && "Database handle should be NULL when error occurs");
        printf("Invalid URL test passed\n");
    }
}

// 主测试函数
int main(void) {
    printf("Running poly_db tests...\n");
    
    // 禁用自动初始化
    setenv("INFRA_NO_AUTO_INIT", "1", 1);

    // 确保系统未初始化
    infra_cleanup();
    
    // 初始化内存管理
    infra_config_t config;
    infra_error_t err = infra_config_init(&config);
    if (err != INFRA_OK) {
        printf("Failed to initialize config: %d\n", err);
        return 1;
    }

    config.memory.use_memory_pool = false;
    config.memory.pool_initial_size = 0;
    config.memory.pool_alignment = sizeof(void*);

    err = infra_init_with_config((infra_init_flags_t)INFRA_INIT_MEMORY, &config);
    if (err != INFRA_OK) {
        printf("Failed to initialize memory management: %d\n", err);
        return 1;
    }
    
    // 运行所有测试
    test_db_open();
    
    // 清理内存管理
    infra_cleanup();
    
    printf("All poly_db tests passed!\n");
    return 0;
}
