#!/bin/bash

# WAL 恢复测试脚本
# 用途：测试 PPDB 的 WAL 恢复机制在各种故障场景下的表现

# 配置
PPDB_BIN="../../build/ppdb"
PPDB_CLI="../../build/ppdb-cli"
TEST_PORT=7000
TEST_DIR="/tmp/ppdb_test_recovery"

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
}

# 清理环境
cleanup() {
    stop_server
    rm -rf "$TEST_DIR"
}

# 测试正常恢复
test_normal_recovery() {
    echo "Testing normal recovery..."
    
    # 写入一些数据
    $PPDB_CLI -p $TEST_PORT put key1 "value1"
    $PPDB_CLI -p $TEST_PORT put key2 "value2"
    $PPDB_CLI -p $TEST_PORT put key3 "value3"
    
    # 正常关闭服务器
    stop_server
    
    # 重启服务器
    start_server
    
    # 验证数据
    for i in {1..3}; do
        VALUE=$($PPDB_CLI -p $TEST_PORT get "key$i")
        if [ "$VALUE" != "value$i" ]; then
            echo "Recovery test failed: key$i expected 'value$i', got '$VALUE'"
            return 1
        fi
    done
    
    echo "Normal recovery test passed"
    return 0
}

# 测试崩溃恢复
test_crash_recovery() {
    echo "Testing crash recovery..."
    
    # 写入一些数据
    $PPDB_CLI -p $TEST_PORT put crash_key1 "crash_value1"
    $PPDB_CLI -p $TEST_PORT put crash_key2 "crash_value2"
    
    # 模拟崩溃（强制终止进程）
    kill -9 $PPDB_PID
    wait $PPDB_PID 2>/dev/null
    
    # 重启服务器
    start_server
    
    # 验证数据
    for i in {1..2}; do
        VALUE=$($PPDB_CLI -p $TEST_PORT get "crash_key$i")
        if [ "$VALUE" != "crash_value$i" ]; then
            echo "Crash recovery test failed: crash_key$i expected 'crash_value$i', got '$VALUE'"
            return 1
        fi
    done
    
    echo "Crash recovery test passed"
    return 0
}

# 测试部分写入恢复
test_partial_write_recovery() {
    echo "Testing partial write recovery..."
    
    # 写入一些数据
    $PPDB_CLI -p $TEST_PORT put partial_key1 "partial_value1"
    
    # 在写入过程中模拟崩溃
    ( sleep 0.1; kill -9 $PPDB_PID ) &
    $PPDB_CLI -p $TEST_PORT put partial_key2 "partial_value2"
    wait $PPDB_PID 2>/dev/null
    
    # 重启服务器
    start_server
    
    # 验证完整写入的数据
    VALUE=$($PPDB_CLI -p $TEST_PORT get "partial_key1")
    if [ "$VALUE" != "partial_value1" ]; then
        echo "Partial write recovery test failed: partial_key1 expected 'partial_value1', got '$VALUE'"
        return 1
    fi
    
    echo "Partial write recovery test passed"
    return 0
}

# 测试空值处理
test_empty_values() {
    echo "Testing empty values recovery..."
    
    # 写入空值
    $PPDB_CLI -p $TEST_PORT put empty_key ""
    
    # 重启服务器
    stop_server
    start_server
    
    # 验证空值
    VALUE=$($PPDB_CLI -p $TEST_PORT get "empty_key")
    if [ "$VALUE" != "" ]; then
        echo "Empty value recovery test failed: expected empty string, got '$VALUE'"
        return 1
    fi
    
    echo "Empty value recovery test passed"
    return 0
}

# 主测试流程
main() {
    init_test
    
    # 运行所有恢复测试
    start_server
    test_normal_recovery || { cleanup; exit 1; }
    
    start_server
    test_crash_recovery || { cleanup; exit 1; }
    
    start_server
    test_partial_write_recovery || { cleanup; exit 1; }
    
    start_server
    test_empty_values || { cleanup; exit 1; }
    
    # 清理
    cleanup
    echo "All recovery tests passed!"
    exit 0
}

# 运行测试
main
