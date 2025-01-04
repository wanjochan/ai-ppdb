# PPDB Peer Design

## 架构概述

PPDB的peer系统采用"三主一从"的基础架构：
- 3个主节点：负责数据分片和写入
- 1个从节点：用于只读查询和数据备份

## 组件设计

### Server 设计
1. **基础服务器**
   - 监听指定端口
   - 处理客户端连接
   - 管理数据存储和访问

2. **主节点服务器**
   - 处理写入请求
   - 维护分片数据
   - 参与集群协调

3. **从节点服务器**
   - 提供只读服务
   - 异步复制数据
   - 负载均衡支持

### CLI 设计
1. **命令行工具**
   ```bash
   ppdb-cli [options] command [arguments]
   ```

2. **基础命令**
   - `connect`: 连接到服务器
   - `get/set`: 基础KV操作
   - `info`: 查看服务器信息
   - `status`: 查看集群状态

3. **管理命令**
   - `cluster`: 集群管理
   - `shard`: 分片操作
   - `replica`: 复制管理
   - `strategy`: 策略控制

4. **监控命令**
   - `monitor`: 实时监控
   - `stats`: 统计信息
   - `logs`: 日志查看

### 交互流程
1. **客户端写入**
   ```
   Client -> CLI -> 主节点 -> 分片存储 -> 从节点同步
   ```

2. **客户端读取**
   ```
   Client -> CLI -> 负载均衡 -> 主节点/从节点
   ```

3. **管理操作**
   ```
   Admin -> CLI -> 管控接口 -> 集群协调 -> 节点执行
   ```

## 分发策略

### 策略类型
1. **分片模式**（默认）
   - 数据通过一致性哈希分布在主节点上
   - 每个主节点负责部分数据
   - 从节点异步复制所有数据

2. **全量复制模式**
   - 所有主节点保存完整数据
   - 适用于读多写少的场景
   - 类似CDN边缘节点模式

### 策略切换
策略切换通过管控命令进行，而不是自动切换，以避免抖动。

## 安全机制

### 预检查系统
```c
typedef struct ppdb_precheck {
    bool system_healthy;      // 系统健康状况
    bool network_stable;      // 网络稳定性
    bool enough_resources;    // 资源充足度
    double estimated_impact;  // 预估影响
} ppdb_precheck_t;
```

### 熔断保护
```c
struct ppdb_circuit_breaker {
    uint32_t error_count;         // 错误计数
    uint32_t slow_request_count;  // 慢请求计数
    time_t last_failure_time;     // 上次失败时间
    bool is_open;                 // 熔断状态
};
```

### 安全网
```c
typedef struct ppdb_safety_net {
    uint32_t max_concurrent_ops;  // 最大并发操作数
    uint32_t max_batch_size;      // 最大批处理大小
    uint32_t timeout_ms;          // 超时时间
    double load_threshold;        // 负载阈值
} ppdb_safety_net_t;
```

## 监控指标

### 基础指标
```c
struct ppdb_metrics {
    uint64_t strategy_switches;   // 策略切换次数
    uint64_t failed_operations;   // 失败操作数
    double avg_switch_time;       // 平均切换时间
    struct timeval last_switch_time;  // 上次切换时间
};
```

## 故障恢复

### 两阶段切换
1. **准备阶段**
   - 系统预检查
   - 资源评估
   - 创建检查点

2. **提交阶段**
   - 执行切换
   - 监控影响
   - 准备回滚点

### 回滚机制
在切换失败时，系统能够：
- 恢复到之前的状态
- 保持数据一致性
- 记录失败原因

## 最佳实践

1. **策略切换时机**
   - 大促前切换到全量模式
   - 维护时进行节点隔离
   - 数据分布不均时重平衡

2. **安全考虑**
   - 避免在高峰期切换
   - 确保有足够资源
   - 保持可回滚性

3. **监控建议**
   - 实时监控系统指标
   - 设置合理的告警阈值
   - 保存详细的操作日志

## 后续规划

1. **智能分发模式**
   - 基于访问模式的智能分片
   - 热点数据的动态调整
   - 自适应的复制策略

2. **更多安全特性**
   - 细粒度的权限控制
   - 更完善的审计日志
   - 自动化的灾难恢复

3. **性能优化**
   - 批量操作的优化
   - 网络传输的优化
   - 存储引擎的优化
