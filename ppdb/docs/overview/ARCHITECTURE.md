# PPDB 系统架构

> 本文档描述了 PPDB 的系统架构、核心组件和关键技术选型。它是理解系统的核心文档。
> 注意阅读 PLAN.md 了解开发进度

## 1. 架构概览

### 1.1 系统分层
```
+------------------------+
|       HTTP API         |
+------------------------+
|    分布式协调层        |
| +-----------------+    |
| |   共识协议(Raft)|    |
| +-----------------+    |
| |   成员管理      |    |
| +-----------------+    |
+------------------------+
|      存储引擎          |
| +-----------------+    |
| | MemTable | WAL  |    |
| +-----------------+    |
| |    SSTable      |    |
| +-----------------+    |
+------------------------+
|      文件系统          |
+------------------------+
```

### 1.2 核心特性
- 完全去中心化：无需中心节点，所有节点地位平等
- 自组织网络：节点可以自动发现、加入和退出
- 跨平台支持：基于 Cosmopolitan 实现全平台兼容
- 高可用性：通过多副本和一致性协议保证数据可靠性
- 可扩展性：支持动态添加节点实现水平扩展

## 2. 核心组件

### 2.1 分布式协调层
- 节点发现和管理
  * 去中心化的节点发现
  * 自动化的成员管理
  * 故障检测和自动恢复
  * 优雅的节点加入/退出

- Raft 共识协议
  * 领导选举
  * 日志复制
  * 成员变更
  * 快照管理

- 数据分片和复制
  * 一致性哈希分片
  * 虚拟节点负载均衡
  * 自动数据迁移
  * 多副本策略

### 2.2 存储引擎
- MemTable
  * 基于跳表实现
  * 支持快照隔离
  * 布隆过滤器优化
  * 并发访问控制

- WAL (Write-Ahead Log)
  * 高性能日志写入
  * 组提交优化
  * 异步刷盘
  * 快速恢复

- SSTable
  * LSM树组织
  * 压缩和编码
  * 索引和过滤器
  * 合并策略

## 3. 关键技术选型

### 3.1 分布式设计
- 一致性协议：Raft
  * 强一致性保证
  * 易于理解和实现
  * 成员变更支持
  * 领导者选举优化

- 数据分片：一致性哈希
  * 最小化数据迁移
  * 负载均衡
  * 虚拟节点支持
  * 自动再平衡

### 3.2 存储设计
- 内存数据结构：跳表
  * 高效的范围查询
  * 并发友好
  * 内存使用优化
  * 迭代器支持

- 持久化存储：LSM树
  * 写入优化
  * 分层合并
  * 空间放大控制
  * 读放大优化

## 4. 数据流

### 4.1 写入流程
1. 客户端请求
2. 路由到目标节点
3. 写入 WAL
4. 更新 MemTable
5. 返回确认

### 4.2 读取流程
1. 客户端请求
2. 路由到目标节点
3. 检查 MemTable
4. 必要时查询 SSTable
5. 返回结果

### 4.3 压缩流程
1. MemTable 达到阈值
2. 转换为不可变 MemTable
3. 后台写入 SSTable
4. 更新元数据
5. 清理旧文件

## 5. 可靠性保证

### 5.1 数据持久性
- WAL 持久化
- 多副本复制
- 校验和验证
- 崩溃恢复

### 5.2 一致性保证
- 强一致性模型
- 快照隔离
- 原子性操作
- 事务支持

### 5.3 可用性保证
- 自动故障转移
- 快速故障检测
- 自动数据修复
- 负载均衡

## 6. 扩展性设计

### 6.1 水平扩展
- 动态节点添加
- 自动数据迁移
- 负载自动均衡
- 容量动态调整

### 6.2 垂直扩展
- 资源动态分配
- 参数自动调优
- 性能监控
- 瓶颈识别 