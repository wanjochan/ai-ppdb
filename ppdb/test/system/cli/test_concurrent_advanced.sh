#!/bin/bash

# PPDB 高级并发测试脚本
# 用途：测试 PPDB 在复杂并发场景下的正确性和稳定性

# 配置
PPDB_BIN="../../build/ppdb"
PPDB_CLI="../../build/ppdb-cli"
TEST_PORT=7000
TEST_DIR="/tmp/ppdb_concurrent_test"
NUM_THREADS=4          # 每个测试场景的并发线程数

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

# 测试并发读写冲突
test_concurrent_rw_conflicts() {
    echo "Testing concurrent read/write conflicts..."
    
    # 预先写入一些数据
    for i in {1..100}; do
        $PPDB_CLI -p $TEST_PORT put "conflict_key_$i" "initial_value_$i"
    done
    
    # 启动多个读取线程
    for i in $(seq 1 $NUM_THREADS); do
        (
            while true; do
                for j in {1..100}; do
                    $PPDB_CLI -p $TEST_PORT get "conflict_key_$j" > /dev/null
                done
            done
        ) &
        READER_PIDS[$i]=$!
    done
    
    # 启动多个写入线程
    for i in $(seq 1 $NUM_THREADS); do
        (
            for j in {1..100}; do
                $PPDB_CLI -p $TEST_PORT put "conflict_key_$j" "new_value_${i}_$j"
                sleep 0.01  # 小延迟，增加冲突机会
            done
        ) &
        WRITER_PIDS[$i]=$!
    done
    
    # 等待写入线程完成
    for pid in "${WRITER_PIDS[@]}"; do
        wait $pid
    done
    
    # 停止读取线程
    for pid in "${READER_PIDS[@]}"; do
        kill $pid
        wait $pid 2>/dev/null
    done
    
    # 验证数据一致性
    for i in {1..100}; do
        VALUE=$($PPDB_CLI -p $TEST_PORT get "conflict_key_$i")
        if [[ ! "$VALUE" =~ ^new_value_[0-9]+_[0-9]+$ ]]; then
            echo "Consistency check failed for conflict_key_$i: $VALUE"
            return 1
        fi
    done
    
    echo "Concurrent read/write conflict test passed"
    return 0
}

# 测试并发恢复
test_concurrent_recovery() {
    echo "Testing concurrent recovery..."
    
    # 启动多个写入线程
    for i in $(seq 1 $NUM_THREADS); do
        (
            for j in {1..50}; do
                $PPDB_CLI -p $TEST_PORT put "recovery_key_${i}_$j" "value_${i}_$j"
            done
        ) &
        WRITER_PIDS[$i]=$!
    done
    
    # 等待一些写入完成后模拟崩溃
    sleep 2
    kill -9 $PPDB_PID
    wait $PPDB_PID 2>/dev/null
    
    # 重启服务器
    start_server
    
    # 验证已写入的数据
    local success=true
    for i in $(seq 1 $NUM_THREADS); do
        for j in {1..50}; do
            VALUE=$($PPDB_CLI -p $TEST_PORT get "recovery_key_${i}_$j")
            if [ ! -z "$VALUE" ] && [ "$VALUE" != "value_${i}_$j" ]; then
                echo "Recovery consistency check failed for recovery_key_${i}_$j"
                success=false
            fi
        done
    done
    
    if [ "$success" = true ]; then
        echo "Concurrent recovery test passed"
        return 0
    else
        echo "Concurrent recovery test failed"
        return 1
    fi
}

# 测试事务隔离
test_transaction_isolation() {
    echo "Testing transaction isolation..."
    
    # 启动多个并发事务
    for i in $(seq 1 $NUM_THREADS); do
        (
            # 模拟一个简单的转账事务
            $PPDB_CLI -p $TEST_PORT put "account_A_$i" "1000"
            $PPDB_CLI -p $TEST_PORT put "account_B_$i" "0"
            
            # 转账操作
            local amount=100
            for j in {1..5}; do
                local a_value=$($PPDB_CLI -p $TEST_PORT get "account_A_$i")
                local b_value=$($PPDB_CLI -p $TEST_PORT get "account_B_$i")
                
                a_value=$((a_value - amount))
                b_value=$((b_value + amount))
                
                $PPDB_CLI -p $TEST_PORT put "account_A_$i" "$a_value"
                $PPDB_CLI -p $TEST_PORT put "account_B_$i" "$b_value"
                
                sleep 0.1  # 增加并发冲突的机会
            done
        ) &
        TRANSACTION_PIDS[$i]=$!
    done
    
    # 等待所有事务完成
    for pid in "${TRANSACTION_PIDS[@]}"; do
        wait $pid
    done
    
    # 验证最终余额
    local success=true
    for i in $(seq 1 $NUM_THREADS); do
        local a_final=$($PPDB_CLI -p $TEST_PORT get "account_A_$i")
        local b_final=$($PPDB_CLI -p $TEST_PORT get "account_B_$i")
        local total=$((a_final + b_final))
        
        if [ "$total" != "1000" ]; then
            echo "Transaction isolation failed for account pair $i: A=$a_final, B=$b_final, Total=$total"
            success=false
        fi
    done
    
    if [ "$success" = true ]; then
        echo "Transaction isolation test passed"
        return 0
    else
        echo "Transaction isolation test failed"
        return 1
    fi
}

# 主测试流程
main() {
    init_test
    
    # 运行所有并发测试
    start_server
    test_concurrent_rw_conflicts || { cleanup; exit 1; }
    
    start_server
    test_concurrent_recovery || { cleanup; exit 1; }
    
    start_server
    test_transaction_isolation || { cleanup; exit 1; }
    
    # 清理
    cleanup
    echo "All concurrent tests passed!"
    exit 0
}

# 运行测试
main
