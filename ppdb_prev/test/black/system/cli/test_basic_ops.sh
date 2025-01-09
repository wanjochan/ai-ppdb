#!/bin/bash

# 基本操作测试脚本
# 用途：测试 PPDB 的基本 CRUD 操作

# 配置
PPDB_BIN="../../build/ppdb"
PPDB_CLI="../../build/ppdb-cli"
TEST_PORT=7000
TEST_DIR="/tmp/ppdb_test"

# 测试初始化
init_test() {
    echo "Initializing test environment..."
    rm -rf "$TEST_DIR"
    mkdir -p "$TEST_DIR"
}

# 启动服务器
start_server() {
    echo "Starting PPDB server..."
    $PPDB_BIN --port $TEST_PORT --data-dir "$TEST_DIR" &
    PPDB_PID=$!
    sleep 2  # 等待服务器启动
}

# 停止服务器
stop_server() {
    echo "Stopping PPDB server..."
    kill $PPDB_PID
    wait $PPDB_PID 2>/dev/null
    rm -rf "$TEST_DIR"
}

# 测试基本操作
test_basic_operations() {
    echo "Testing basic operations..."
    
    # 测试PUT
    echo "Testing PUT operation..."
    $PPDB_CLI -p $TEST_PORT put test_key "test_value"
    if [ $? -ne 0 ]; then
        echo "PUT operation failed"
        return 1
    fi
    
    # 测试GET
    echo "Testing GET operation..."
    VALUE=$($PPDB_CLI -p $TEST_PORT get test_key)
    if [ "$VALUE" != "test_value" ]; then
        echo "GET operation failed: expected 'test_value', got '$VALUE'"
        return 1
    fi
    
    # 测试DELETE
    echo "Testing DELETE operation..."
    $PPDB_CLI -p $TEST_PORT delete test_key
    if [ $? -ne 0 ]; then
        echo "DELETE operation failed"
        return 1
    fi
    
    # 验证删除
    VALUE=$($PPDB_CLI -p $TEST_PORT get test_key)
    if [ $? -eq 0 ]; then
        echo "Key still exists after DELETE"
        return 1
    fi
    
    return 0
}

# 主测试流程
main() {
    init_test
    start_server
    
    # 运行测试
    if test_basic_operations; then
        echo "All basic operations tests passed!"
        RESULT=0
    else
        echo "Some tests failed!"
        RESULT=1
    fi
    
    stop_server
    exit $RESULT
}

main
