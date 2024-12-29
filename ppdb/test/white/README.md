# PPDB 白盒测试

本目录包含PPDB（持久化键值存储）的白盒测试套件。测试套件分为三个主要部分：WAL、MemTable和KVStore。

## 测试结构

```
test_white/
├── test_framework.h     # 测试框架头文件
├── test_framework.c     # 测试框架实现
├── test_wal.c          # WAL测试用例
├── test_wal_main.c     # WAL测试入口
├── test_memtable.c     # MemTable测试用例
├── test_memtable_main.c # MemTable测试入口
├── test_kvstore.c      # KVStore测试用例
└── test_kvstore_main.c # KVStore测试入口
```

## 测试套件说明

### WAL测试
- 测试WAL的基本操作（创建、写入、读取、恢复）
- 测试文件系统操作
- 测试错误处理

### MemTable测试
- 测试基本的键值操作（插入、查询、删除）
- 测试内存限制
- 测试迭代器功能

### KVStore测试
- 测试基本操作（创建、打开、关闭）
- 测试键值操作（Put、Get、Delete）
- 测试数据恢复功能
- 测试并发操作

## 测试计划

### 1. 基础组件测试
- [x] 基本数据结构测试 (test_basic.c)
  - [x] 跳表基本操作
  - [x] 哈希表操作
  - [ ] 其他基础数据结构

- [x] 无锁数据结构测试 (test_atomic_skiplist.c)
  - [x] 基本操作验证
  - [x] 并发正确性
  - [ ] 内存屏障和原子性验证
  - [ ] ABA问题测试
  - [ ] 性能对比测试

- [x] 迭代器测试 (test_iterator.c)
  - [x] 基本遍历
  - [ ] 并发遍历
  - [ ] 快照读

### 2. 核心功能测试
- [x] MemTable测试 (test_memtable.c)
  - [x] 基本CRUD操作
  - [x] 内存限制
  - [ ] 并发写入
  - [ ] 溢出处理
  - [ ] 性能基准

- [x] WAL测试 (test_wal.c)
  - [x] 基本写入和恢复
  - [ ] 并发写入 (test_wal_concurrent.c 待实现)
  - [ ] 归档和清理
  - [ ] 故障恢复
  - [ ] 性能测试

- [x] KVStore测试 (test_kvstore.c)
  - [x] 基本CRUD操作
  - [x] 事务支持
  - [ ] 并发控制
  - [ ] 恢复机制
  - [ ] 性能测试

### 3. 特殊场景测试
- [x] 并发测试 (test_concurrent.c)
  - [x] 多线程读写
  - [ ] 死锁检测
  - [ ] 资源竞争

- [x] 边界条件测试 (test_edge.c)
  - [x] 极限值测试
  - [ ] 错误注入
  - [ ] 资源耗尽

- [ ] 压力测试 (待实现)
  - [ ] 高并发压测
  - [ ] 大数据量测试
  - [ ] 长时间稳定性

### 4. 性能和监控测试
- [x] 指标测试 (test_metrics.c)
  - [x] 基本指标收集
  - [ ] 性能监控
  - [ ] 资源使用统计

- [ ] 性能基准测试 (待实现)
  - [ ] 吞吐量测试
  - [ ] 延迟测试
  - [ ] 资源消耗测试

### 5. 故障测试
- [ ] 故障注入测试 (待实现)
  - [ ] 进程崩溃
  - [ ] 磁盘故障
  - [ ] 网络分区

- [ ] 恢复测试 (待实现)
  - [ ] 状态恢复
  - [ ] 数据一致性
  - [ ] 部分故障恢复

## 测试优先级
1. 高优先级（必须完成）
   - WAL并发测试实现
   - 压力测试框架
   - 基本故障恢复测试

2. 中优先级
   - 性能基准测试
   - 完整的边界条件测试
   - 资源监控测试

3. 低优先级
   - 高级故障注入
   - 复杂场景测试
   - 扩展性测试

## 运行测试

在项目根目录下执行以下命令：

```bash
# 构建并运行所有测试
./scripts/build_test_old.bat

# 或者单独运行各个测试
./build/test/test_wal.exe      # 运行WAL测试
./build/test/test_memtable.exe # 运行MemTable测试
./build/test/test_kvstore.exe  # 运行KVStore测试
```

## 测试输出说明

测试输出包含以下信息：
- 测试套件名称
- 测试用例名称和结果
- 详细的错误信息（如果测试失败）
- 操作统计信息

示例输出：
```
Starting WAL tests...
[INFO] Test suite: WAL
[INFO] Running test: create_close ... OK
[INFO] Running test: basic_ops ... OK
[INFO] Running test: recovery ... OK
WAL tests completed with result: 0
```

## 注意事项

1. 每次测试会自动清理测试目录
2. 测试过程中会创建临时文件，请确保有足够的磁盘空间
3. 并发测试会创建多个线程，可能需要较多系统资源