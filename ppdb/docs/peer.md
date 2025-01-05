# PPDB 节点层文档

## 概述

节点层负责管理PPDB的分布式功能，包括：

- 节点发现和管理
- 集群通信
- 一致性协议
- 分布式事务

## 节点管理

### 节点状态

```c
typedef enum ppdb_peer_state_e {
    PPDB_PEER_STATE_INIT,       // 初始化
    PPDB_PEER_STATE_JOINING,    // 正在加入集群
    PPDB_PEER_STATE_ACTIVE,     // 活跃状态
    PPDB_PEER_STATE_LEAVING,    // 正在离开集群
    PPDB_PEER_STATE_INACTIVE    // 非活跃状态
} ppdb_peer_state_t;
```

### 节点配置

```c
typedef struct ppdb_peer_config_s {
    const char* host;           // 主机地址
    uint16_t port;             // 端口号
    const char* cluster_id;     // 集群ID
    const char* node_id;        // 节点ID
    size_t heartbeat_interval;  // 心跳间隔（毫秒）
    size_t election_timeout;    // 选举超时（毫秒）
} ppdb_peer_config_t;
```

## 集群通信

### 消息类型

```c
typedef enum ppdb_peer_msg_type_e {
    PPDB_PEER_MSG_HEARTBEAT,    // 心跳消息
    PPDB_PEER_MSG_VOTE,         // 投票消息
    PPDB_PEER_MSG_APPEND,       // 日志追加
    PPDB_PEER_MSG_SNAPSHOT,     // 快照传输
    PPDB_PEER_MSG_COMMAND       // 命令消息
} ppdb_peer_msg_type_t;
```

### 通信接口

```c
ppdb_error_t ppdb_peer_send(ppdb_peer_t* peer, const void* data, size_t size);
ppdb_error_t ppdb_peer_recv(ppdb_peer_t* peer, void* buffer, size_t* size);
ppdb_error_t ppdb_peer_broadcast(ppdb_peer_t* peer, const void* data, size_t size);
```

## 一致性协议

### Raft状态机

```c
typedef enum ppdb_peer_role_e {
    PPDB_PEER_ROLE_FOLLOWER,   // 跟随者
    PPDB_PEER_ROLE_CANDIDATE,  // 候选者
    PPDB_PEER_ROLE_LEADER      // 领导者
} ppdb_peer_role_t;
```

### 日志复制

```c
ppdb_error_t ppdb_peer_append_entries(ppdb_peer_t* peer, const ppdb_peer_entry_t* entries, size_t count);
ppdb_error_t ppdb_peer_apply_entries(ppdb_peer_t* peer, uint64_t commit_index);
```

## 分布式事务

### 事务状态

```c
typedef enum ppdb_peer_txn_state_e {
    PPDB_PEER_TXN_INIT,        // 初始化
    PPDB_PEER_TXN_PREPARING,   // 准备中
    PPDB_PEER_TXN_PREPARED,    // 已准备
    PPDB_PEER_TXN_COMMITTING,  // 提交中
    PPDB_PEER_TXN_COMMITTED,   // 已提交
    PPDB_PEER_TXN_ABORTING,    // 回滚中
    PPDB_PEER_TXN_ABORTED      // 已回滚
} ppdb_peer_txn_state_t;
```

### 两阶段提交

```c
ppdb_error_t ppdb_peer_txn_prepare(ppdb_peer_t* peer, const char* txn_id);
ppdb_error_t ppdb_peer_txn_commit(ppdb_peer_t* peer, const char* txn_id);
ppdb_error_t ppdb_peer_txn_abort(ppdb_peer_t* peer, const char* txn_id);
```

## 最佳实践

1. **节点管理**
   - 定期检查节点健康状态
   - 及时清理失效节点
   - 合理配置超时参数

2. **网络通信**
   - 使用异步I/O提高性能
   - 实现消息重传机制
   - 处理网络分区情况

3. **一致性保证**
   - 正确实现Raft协议
   - 确保日志一致性
   - 处理成员变更

4. **故障恢复**
   - 实现快照机制
   - 定期备份状态
   - 支持增量恢复

## 错误处理

所有函数返回`ppdb_error_t`，可能的值包括：

- `PPDB_OK`：成功
- `PPDB_ERR_NETWORK`：网络错误
- `PPDB_ERR_TIMEOUT`：操作超时
- `PPDB_ERR_CONSENSUS`：一致性错误
- `PPDB_ERR_PARTITION`：网络分区
- `PPDB_ERR_STATE`：状态错误

## 监控指标

- 节点状态
- 网络延迟
- 日志同步延迟
- 选举频率
- 事务成功率
- 网络吞吐量