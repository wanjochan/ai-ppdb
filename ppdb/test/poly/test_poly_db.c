#include "internal/poly/poly_db.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// 测试 URL 解析和数据库连接
static void test_db_open(void) {
    printf("Testing database open :memory: \n");
    
    // 测试 SQLite
    {
        poly_db_t* db = NULL;
        infra_error_t err = poly_db_open("sqlite://:memory:", &db);
        assert(err == INFRA_OK && "Failed to open SQLite database");
        assert(db != NULL && "Database handle is NULL");
        poly_db_close(db);
        printf("SQLite test passed\n");
    }
    
    // 测试 DuckDB
    {
        poly_db_t* db = NULL;
        infra_error_t err = poly_db_open("duckdb://:memory:", &db);
        assert(err == INFRA_OK && "Failed to open DuckDB database");
        assert(db != NULL && "Database handle is NULL");
        poly_db_close(db);
        printf("DuckDB test passed\n");
    }
    
    // 测试无效 URL
    {
        poly_db_t* db = NULL;
        infra_error_t err = poly_db_open("invalid://:memory:", &db);
        assert(err == INFRA_ERROR_INVALID_PARAM && "Should fail with invalid scheme");
        printf("Invalid URL test passed\n");
    }
}

// 主测试函数
int main(void) {
    printf("Running poly_db tests...\n");
    
    // 运行所有测试
    test_db_open();
    
    printf("All poly_db tests passed!\n");
    return 0;
}
