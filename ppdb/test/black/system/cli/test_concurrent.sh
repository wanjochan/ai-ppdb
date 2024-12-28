#!/bin/bash

# 并发操作测试脚本
# 用途：测试 PPDB 在并发场景下的表现

# 配置
PPDB_BIN="../../build/ppdb"
PPDB_CLI="../../build/ppdb-cli"
TEST_PORT=7000
TEST_DIR="/tmp/ppdb_test"
NUM_CLIENTS=10
OPS_PER_CLIENT=100

# 测试初始化
init_test() {
    echo "Initializing concurrent test environment..."
    rm -rf "$TEST_DIR"
    mkdir -p "$TEST_DIR"
}

# 启动服务器
start_server() {
    echo "Starting PPDB server..."
    $PPDB_BIN --port $TEST_PORT --data-dir "$TEST_DIR" &
    PPDB_PID=$!
    sleep 2
}

# 停止服务器
stop_server() {
    echo "Stopping PPDB server..."
    kill $PPDB_PID
    wait $PPDB_PID 2>/dev/null
    rm -rf "$TEST_DIR"
}

# 单个客户端的操作
client_operations() {
    local CLIENT_ID=$1
    local SUCCESS=0
    local FAILED=0
    
    for ((i=1; i<=OPS_PER_CLIENT; i++)); do
        # 写入数据
        $PPDB_CLI -p $TEST_PORT put "key_${CLIENT_ID}_${i}" "value_${CLIENT_ID}_${i}"
        if [ $? -eq 0 ]; then
            SUCCESS=$((SUCCESS+1))
        else
            FAILED=$((FAILED+1))
        fi
        
        # 读取数据
        VALUE=$($PPDB_CLI -p $TEST_PORT get "key_${CLIENT_ID}_${i}")
        if [ "$VALUE" = "value_${CLIENT_ID}_${i}" ]; then
            SUCCESS=$((SUCCESS+1))
        else
            FAILED=$((FAILED+1))
        fi
    done
    
    echo "Client $CLIENT_ID: Success=$SUCCESS, Failed=$FAILED"
}

# 运行并发测试
run_concurrent_test() {
    echo "Starting concurrent test with $NUM_CLIENTS clients..."
    
    # 启动多个并发客户端
    for ((i=1; i<=NUM_CLIENTS; i++)); do
        client_operations $i &
    done
    
    # 等待所有客户端完成
    wait
}

# 主测试流程
main() {
    init_test
    start_server
    
    # 运行并发测试
    run_concurrent_test
    
    stop_server
}

main
