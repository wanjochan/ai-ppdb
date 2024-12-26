#!/bin/bash

# PPDB 错误注入测试脚本
# 用途：通过注入各种故障来测试系统的鲁棒性

# 配置
PPDB_BIN="../../build/ppdb"
PPDB_CLI="../../build/ppdb-cli"
TEST_PORT=7000
TEST_DIR="/tmp/ppdb_fault_test"

# 测试初始化
init_test() {
    echo "Initializing fault injection test environment..."
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

# 模拟磁盘写入错误
test_disk_write_errors() {
    echo "Testing disk write errors..."
    
    # 使文件系统只读
    mount -o remount,ro "$TEST_DIR" 2>/dev/null || true
    
    # 尝试写入数据
    local success=true
    for i in {1..10}; do
        if $PPDB_CLI -p $TEST_PORT put "disk_error_key_$i" "value_$i" 2>/dev/null; then
            echo "Write should have failed due to read-only filesystem"
            success=false
            break
        fi
    done
    
    # 恢复文件系统
    mount -o remount,rw "$TEST_DIR" 2>/dev/null || true
    
    if [ "$success" = true ]; then
        echo "Disk write error test passed"
        return 0
    else
        echo "Disk write error test failed"
        return 1
    fi
}

# 模拟内存不足
test_memory_pressure() {
    echo "Testing memory pressure..."
    
    # 使用 cgroup 限制内存（如果可用）
    if [ -d "/sys/fs/cgroup/memory" ]; then
        echo $$ > /sys/fs/cgroup/memory/ppdb_test/tasks
        echo $((50*1024*1024)) > /sys/fs/cgroup/memory/ppdb_test/memory.limit_in_bytes
    fi
    
    # 尝试写入大量数据
    local large_value=$(head -c 10M /dev/urandom | base64)
    local success=true
    
    for i in {1..10}; do
        if ! $PPDB_CLI -p $TEST_PORT put "large_key_$i" "$large_value" 2>/dev/null; then
            echo "Write failed as expected under memory pressure"
        else
            # 验证数据完整性
            local retrieved_value=$($PPDB_CLI -p $TEST_PORT get "large_key_$i")
            if [ "$retrieved_value" != "$large_value" ]; then
                echo "Data corruption detected under memory pressure"
                success=false
                break
            fi
        fi
    done
    
    # 清理 cgroup 设置
    if [ -d "/sys/fs/cgroup/memory" ]; then
        echo $$ > /sys/fs/cgroup/memory/tasks
    fi
    
    if [ "$success" = true ]; then
        echo "Memory pressure test passed"
        return 0
    else
        echo "Memory pressure test failed"
        return 1
    fi
}

# 模拟网络故障
test_network_failures() {
    echo "Testing network failures..."
    
    # 使用 iptables 模拟网络故障
    iptables -A INPUT -p tcp --dport $TEST_PORT -j DROP 2>/dev/null || true
    
    # 尝试连接
    local success=true
    if $PPDB_CLI -p $TEST_PORT get "test_key" 2>/dev/null; then
        echo "Connection should have failed due to network block"
        success=false
    fi
    
    # 恢复网络
    iptables -D INPUT -p tcp --dport $TEST_PORT -j DROP 2>/dev/null || true
    
    # 验证服务恢复
    sleep 2
    if ! $PPDB_CLI -p $TEST_PORT put "recovery_key" "test_value"; then
        echo "Service did not recover after network restoration"
        success=false
    fi
    
    if [ "$success" = true ]; then
        echo "Network failure test passed"
        return 0
    else
        echo "Network failure test failed"
        return 1
    fi
}

# 测试损坏的WAL文件
test_corrupted_wal() {
    echo "Testing corrupted WAL handling..."
    
    # 写入一些数据
    for i in {1..10}; do
        $PPDB_CLI -p $TEST_PORT put "wal_key_$i" "value_$i"
    done
    
    # 停止服务器
    stop_server
    
    # 损坏 WAL 文件
    find "$TEST_DIR" -name "wal.*" -exec sh -c 'echo "CORRUPTED" >> {}' \;
    
    # 重启服务器，应该能够检测到损坏并进行恢复
    start_server
    
    # 验证数据一致性
    local success=true
    for i in {1..10}; do
        local value=$($PPDB_CLI -p $TEST_PORT get "wal_key_$i")
        if [ ! -z "$value" ] && [ "$value" != "value_$i" ]; then
            echo "Data inconsistency detected after WAL corruption"
            success=false
            break
        fi
    done
    
    if [ "$success" = true ]; then
        echo "Corrupted WAL test passed"
        return 0
    else
        echo "Corrupted WAL test failed"
        return 1
    fi
}

# 测试文件描述符耗尽
test_fd_exhaustion() {
    echo "Testing file descriptor exhaustion..."
    
    # 获取当前文件描述符限制
    local fd_limit=$(ulimit -n)
    
    # 临时降低文件描述符限制
    ulimit -n 32
    
    # 尝试并发打开多个连接
    local success=true
    for i in {1..50}; do
        if ! $PPDB_CLI -p $TEST_PORT put "fd_key_$i" "value_$i" 2>/dev/null; then
            # 预期部分请求会失败，但服务器不应崩溃
            continue
        fi
    done
    
    # 恢复文件描述符限制
    ulimit -n $fd_limit
    
    # 验证服务器仍然响应
    if ! $PPDB_CLI -p $TEST_PORT put "test_key" "test_value"; then
        echo "Server not responding after FD exhaustion"
        success=false
    fi
    
    if [ "$success" = true ]; then
        echo "File descriptor exhaustion test passed"
        return 0
    else
        echo "File descriptor exhaustion test failed"
        return 1
    fi
}

# 主测试流程
main() {
    init_test
    
    # 运行所有错误注入测试
    start_server
    test_disk_write_errors || { cleanup; exit 1; }
    
    start_server
    test_memory_pressure || { cleanup; exit 1; }
    
    start_server
    test_network_failures || { cleanup; exit 1; }
    
    start_server
    test_corrupted_wal || { cleanup; exit 1; }
    
    start_server
    test_fd_exhaustion || { cleanup; exit 1; }
    
    # 清理
    cleanup
    echo "All fault injection tests passed!"
    exit 0
}

# 运行测试
main
