# PPDB 共识协议实现

> 本文档详细说明了 PPDB 中 Raft 共识协议的实现方案。它是分布式一致性的核心实现文档。相关文档：
> - 分布式架构见 `design/distributed.md`
> - 成员管理见 `design/membership.md`
> - 整体设计见 `overview/DESIGN.md`

## 1. 概述

PPDB采用Raft共识协议来保证分布式系统中的数据一致性。实现了完整的Raft协议，包括领导选举、日志复制和成员变更。

## 2. 领导选举

### 2.1 选举机制
- 基于随机选举超时
- 预投票机制(Pre-Vote)
- 优先级投票
- 选举安全性保证

### 2.2 任期管理
- 任期号单调递增
- 任期持久化存储
- 任期冲突处理
- 任期回退处理

## 3. 日志复制

### 3.1 日志格式
```c
struct raft_log_entry {
    uint64_t term;        // 任期号
    uint64_t index;       // 日志索引
    uint32_t type;        // 日志类型
    uint32_t data_size;   // 数据大小
    uint8_t  data[];      // 日志数据
};
```

### 3.2 复制流程
- 日志追加
- 一致性检查
- 提交确认
- 应用状态机

### 3.3 日志压缩
- 快照机制
- 增量压缩
- 快照传输
- 状态恢复

## 4. 成员变更

### 4.1 单节点变更
- 新增节点
- 移除节点
- 配置变更日志
- 变更状态追踪

### 4.2 联合共识
- 两阶段配置变更
- 过渡期处理
- 安全性保证
- 故障恢复

## 5. 优化设计

### 5.1 性能优化
- 批量日志复制
- 管道化复制
- 异步应用
- 并行复制

### 5.2 可用性优化
- 只读请求优化
- 租约机制
- 领导者转移
- 快速恢复

## 6. 安全性保证

### 6.1 选举安全性
- 单任期单领导
- 日志匹配检查
- 选举限制

### 6.2 状态机安全性
- 日志顺序性
- 提交规则
- 应用顺序

## 7. 实现细节

### 7.1 状态定义
```c
typedef enum {
    RAFT_FOLLOWER,
    RAFT_CANDIDATE,
    RAFT_LEADER,
    RAFT_PRE_CANDIDATE
} raft_state_t;
```

### 7.2 RPC接口
- RequestVote RPC
- AppendEntries RPC
- InstallSnapshot RPC
- PreVote RPC
