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