# 需要修改的文件列表

## 1. 核心文件

### 1.1 对外接口
- `include/ppdb/ppdb.h`
  - 已有基础结构定义
  - 已有类型系统定义
  - 已有统一接口声明

### 1.2 内部实现
- `src/ppdb_internal.h`
  - 内部数据结构定义
  - 内部函数声明
  - 共享工具函数

- `src/base.c`
  - 完善基础结构实现
  - 完善类型系统支持
  - 完善引用计数

- `src/storage.c`
  - 完善skiplist实现
  - 添加memtable支持
  - 添加sharded支持
  - 统一的存储接口

## 2. 测试文件

### 2.1 单元测试
- `test/white/test_base.c`
  - 类型系统测试
  - 内存布局测试
  - 基础功能测试

- `test/white/test_storage.c`
  - skiplist基本操作
  - memtable功能测试
  - sharded功能测试
  - 类型转换测试

### 2.2 性能测试
- `test/perf/test_storage_perf.c`
  - skiplist性能测试
  - memtable性能测试
  - sharded性能测试
  - 内存使用测试

## 3. 文档文件

### 3.1 设计文档
- `docs/design/storage.md`
  - 统一存储设计
  - 类型系统说明
  - 实现细节

### 3.2 测试文档
- `docs/test/PLAN.md`
  - 测试策略
  - 测试用例
  - 性能指标 