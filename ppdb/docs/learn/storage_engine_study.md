# 存储引擎学习笔记

## 1. LSM树研究

### 1.1 基本原理
```markdown
LSM (Log-Structured Merge-tree) 树是一种针对写入优化的数据结构:

1. 核心思想
   - 将随机写转换为顺序写
   - 分层存储减少磁盘开销
   - 批量合并提高效率

2. 关键组件
   - MemTable: 内存中的有序表
   - WAL: 写前日志保证持久性
   - SSTable: 磁盘上的有序表
   - Compaction: 文件合并机制
```

### 1.2 优化策略
```markdown
1. 写入优化
   - 使用跳表实现MemTable
   - WAL批量写入
   - 异步Compaction

2. 读取优化
   - 布隆过滤器
   - 分层缓存
   - 索引和压缩
```

## 2. SSTable深入分析

### 2.1 文件格式优化
```markdown
1. 数据块设计
   - 可变长度键值对
   - 前缀压缩
   - 分块压缩

2. 索引策略
   - 稀疏索引减少内存占用
   - 二分查找加速检索
   - 布隆过滤器过滤不存在的键
```

### 2.2 性能考虑
```markdown
1. 写入性能
   - 顺序写入
   - 批量提交
   - 异步刷盘

2. 读取性能
   - 缓存管理
   - 预读优化
   - 并发控制
```

## 3. 实现要点

### 3.1 关键数据结构
```c
// MemTable跳表节点
struct SkipListNode {
    Key   key;
    Value value;
    int   height;
    Node* next[];
};

// SSTable数据块
struct DataBlock {
    uint32_t magic;      // 块标识
    uint16_t count;      // 记录数
    uint32_t size;       // 块大小
    byte[]   data;       // 实际数据
    uint32_t checksum;   // 校验和
};
```

### 3.2 核心算法
```markdown
1. 写入流程
   - 检查MemTable容量
   - 写入WAL日志
   - 更新MemTable
   - 必要时触发Compaction

2. 读取流程
   - 检查MemTable
   - 查询布隆过滤器
   - 二分查找数据块
   - 解压和返回数据

3. Compaction策略
   - 选择合并文件
   - 多路归并排序
   - 处理重复键值
   - 更新元数据
```

## 4. 性能优化

### 4.1 写入优化
```markdown
1. 批量写入
   - 合并多个写操作
   - 减少WAL同步
   - 优化内存分配

2. 压缩优化
   - 自适应压缩级别
   - 分块压缩
   - 压缩算法选择
```

### 4.2 读取优化
```markdown
1. 缓存管理
   - 块缓存
   - 索引缓存
   - 布隆过滤器缓存

2. IO优化
   - 预读取
   - 异步读取
   - 批量读取
```

## 5. 测试计划

### 5.1 功能测试
```markdown
1. 基本操作
   - Put/Get/Delete
   - 范围查询
   - 迭代器

2. 异常处理
   - 并发冲突
   - 磁盘错误
   - 进程崩溃
```

### 5.2 性能测试
```markdown
1. 基准测试
   - 写入吞吐量
   - 读取延迟
   - 空间占用

2. 压力测试
   - 高并发
   - 大数据量
   - 长时间运行
```

## 6. 待解决问题

### 6.1 技术难点
```markdown
1. Compaction优化
   - 如何选择最佳合并策略
   - 如何减少写放大
   - 如何控制空间放大

2. 缓存管理
   - 缓存更新策略
   - 内存使用控制
   - 缓存一致性
```

### 6.2 后续计划
```markdown
1. 近期任务
   - 完善SSTable实现
   - 优化性能指标
   - 补充单元测试

2. 长期目标
   - 支持事务
   - 实现快照隔离
   - 优化资源使用
``` 