# PPDB 存储设计

## 1. 整体架构

### 1.1 存储层次
```
+----------------+
|    Memtable    |  内存层
+----------------+
|    SSTable     |  磁盘层 (diskv阶段)
+----------------+
|      WAL       |  日志层 (diskv阶段)
+----------------+
```

### 1.2 组件职责
- Memtable: 内存中的有序表，基于skiplist实现
- SSTable: 磁盘上的静态有序表 (diskv阶段)
- WAL: 写前日志，保证持久性 (diskv阶段)

## 2. Memtable 设计

### 2.1 数据结构
- 基于skiplist实现的有序表
- 支持并发读写
- 内存限制和淘汰机制

### 2.2 主要操作
- Get: O(log N) 复杂度查询
- Put: O(log N) 复杂度插入
- Delete: O(log N) 复杂度删除
- Scan: 范围扫描支持

### 2.3 内存管理
- 固定内存上限
- LRU淘汰策略
- 内存使用统计

## 3. 分片管理

### 3.1 分片策略
- 一致性哈希
- 动态分片调整
- 负载均衡

### 3.2 数据分布
- 均匀分布
- 热点避免
- 分片迁移

### 3.3 分片同步
- 主从复制
- 数据一致性
- 故障恢复

## 4. 性能优化

### 4.1 并发控制
- 细粒度锁
- 无锁操作
- 原子更新

### 4.2 内存优化
- 内存对齐
- 缓存友好
- 内存池管理

### 4.3 读写优化
- 批量操作
- 预读取
- 写入缓冲

## 5. 监控指标

### 5.1 性能指标
- 读写延迟
- 吞吐量
- 命中率

### 5.2 资源指标
- 内存使用
- CPU使用
- 磁盘IO (diskv阶段)

### 5.3 错误指标
- 错误率
- 超时率
- 拒绝率 