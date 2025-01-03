# 源码修改计划

## 1. 基础结构完善

### 1.1 统一基础结构
```c
// ppdb.h中已定义
typedef struct {
    ppdb_header_t header;     // 4字节
    union {
        struct {
            union {
                void* head;    // skiplist
                int fd;        // wal/sst
            };
            union {
                void* pool;    // skiplist
                void* buffer;  // wal/cache
            };
        } storage;
        struct {
            size_t limit;      // memtable
            atomic_size_t used;
        } mem;
        struct {
            uint32_t count;
            void** ptrs;       // shards/sstables
        } array;
    };
} ppdb_base_t;
```

### 1.2 类型系统
```c
// ppdb.h中已定义
typedef enum {
    PPDB_TYPE_SKIPLIST = 1,    // 基础跳表
    PPDB_TYPE_MEMTABLE = 2,    // 内存表
    PPDB_TYPE_SHARDED = 4,     // 分片表
    PPDB_TYPE_WAL = 8,         // 预写日志
    PPDB_TYPE_SSTABLE = 16     // 有序表
} ppdb_type_t;
```

## 2. 实现计划

### 2.1 第一阶段：skiplist完善（2周）

#### 2.1.1 基础实现
1. 完善skiplist_init
   - 初始化存储级别锁
   - 初始化头节点
   - 设置类型标记

2. 完善skiplist_destroy
   - 清理节点数据
   - 销毁锁
   - 释放内存

3. 完善基本操作
   - skiplist_get
   - skiplist_put
   - skiplist_remove

#### 2.1.2 并发控制
1. 读写锁
   - 存储级别锁
   - 节点级别锁
   - 锁升级/降级

2. 无锁优化
   - CAS操作
   - 内存屏障
   - ABA问题处理

### 2.2 第二阶段：memtable实现（2周）

#### 2.2.1 基础结构
1. 完善memtable_init
   - 内存限制设置
   - 使用计数初始化
   - 类型标记设置

2. 完善memtable_destroy
   - 清理数据
   - 重置计数
   - 释放资源

3. 基本操作实现
   - memtable_get
   - memtable_put
   - memtable_remove

#### 2.2.2 内存管理
1. 使用统计
   - 内存使用计数
   - 容量检查
   - 溢出处理

2. 性能优化
   - 内存对齐
   - 缓存友好
   - 内存池

### 2.3 第三阶段：sharded实现（2周）

#### 2.3.1 基础结构
1. 完善sharded_init
   - 分片数组初始化
   - 分片锁初始化
   - 类型标记设置

2. 完善sharded_destroy
   - 清理分片
   - 销毁锁
   - 释放资源

3. 基本操作实现
   - sharded_get
   - sharded_put
   - sharded_remove

#### 2.3.2 分片管理
1. 分片策略
   - 哈希分片
   - 动态调整
   - 负载均衡

2. 并发控制
   - 分片锁
   - 跨分片操作
   - 死锁预防

## 3. 测试计划

### 3.1 单元测试
1. 基础测试
   - 类型系统
   - 内存布局
   - 基本操作

2. 并发测试
   - 多线程读写
   - 锁竞争
   - 死锁检测

3. 性能测试
   - 基准测试
   - 内存使用
   - 扩展性测试

### 3.2 集成测试
1. 组件交互
   - skiplist + memtable
   - memtable + sharded
   - 完整流程

2. 压力测试
   - 高并发
   - 大数据量
   - 长时间运行

## 4. 时间安排

### 4.1 第一周
- skiplist基础实现
- 基本操作完善
- 单元测试

### 4.2 第二周
- skiplist并发控制
- 性能优化
- 压力测试

### 4.3 第三周
- memtable基础实现
- 内存管理
- 功能测试

### 4.4 第四周
- memtable性能优化
- 溢出处理
- 压力测试

### 4.5 第五周
- sharded基础实现
- 分片策略
- 功能测试

### 4.6 第六周
- sharded并发优化
- 负载均衡
- 集成测试

## 5. 验收标准

### 5.1 功能验收
- 类型系统正确
- 内存布局正确
- 接口完整性
- 并发安全性

### 5.2 性能验收
- 单线程写入>100K ops/s
- 单线程读取>500K ops/s
- 多线程扩展比>0.7
- 内存开销<200%

### 5.3 代码质量
- 代码覆盖率>90%
- 注释完善
- 接口文档完整 