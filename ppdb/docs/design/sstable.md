# PPDB SSTable 设计

> 本文档详细说明了 PPDB 的 SSTable（Sorted String Table）设计和实现。它是存储引擎的持久化组件，负责磁盘数据的组织和管理。相关文档：
> - MemTable 设计见 `design/memtable.md`
> - WAL 设计见 `design/wal.md`
> - 整体设计见 `overview/DESIGN.md`

## 1. 概述

SSTable (Sorted String Table) 是 PPDB 的持久化存储格式，用于将内存中的数据以有序的方式存储到磁盘上。它不是临时的存储空间，而是数据的最终持久化形式。

### 1.1 角色定位
- 永久性存储：作为数据的最终持久化存储形式
- 有序性：所有数据按键有序存储
- 不可变性：一旦写入就不再修改，只能合并或删除
- 可靠性：通过校验和等机制保证数据完整性

### 1.2 数据流转
```
                    ┌─────────┐
写入请求 → WAL日志 →│MemTable │ (内存层：临时缓冲)
                    └────┬────┘
                         ↓
              ┌──────────────────┐
              │     SSTable     │ (磁盘层：永久存储)
              └──────────────────┘
```

### 1.3 设计目标
- 高效的顺序写入
- 快速的随机读取
- 支持范围扫描
- 节省存储空间
- 易于压缩和缓存

### 1.4 与MemTable协作
- 作为MemTable数据的最终归宿
- 通过后台自动刷盘机制接收数据
- 支持手动控制选项
- 提供数据恢复能力

## 2. 文件格式

### 2.1 文件布局
```
+----------------+
|    文件头     |
+----------------+
|    数据块1    |
+----------------+
|    数据块2    |
+----------------+
|     ...       |
+----------------+
|    索引块     |
+----------------+
|    元数据块   |
+----------------+
|    文件尾     |
+----------------+
```

### 2.2 文件头格式
```c
struct SSTableHeader {
    uint32_t magic;          // 魔数：0x53535442 ("SSTB")
    uint32_t version;        // 版本号
    uint64_t sequence;       // 序列号
    uint32_t block_count;    // 数据块数量
    uint32_t index_offset;   // 索引块偏移
    uint32_t meta_offset;    // 元数据块偏移
    uint32_t crc32;         // 校验和
};
```

### 2.3 数据块格式
```c
struct DataBlock {
    uint32_t size;          // 块大小
    uint32_t entry_count;   // 条目数量
    uint8_t  data[];        // 实际数据
    uint32_t crc32;         // 校验和
};

struct DataEntry {
    uint16_t key_size;      // 键长度
    uint32_t value_size;    // 值长度
    uint8_t  key[];         // 键数据
    uint8_t  value[];       // 值数据
};
```

## 3. 自动协作机制

### 3.1 触发条件
```c
// 以下任一条件满足时，自动触发MemTable到SSTable的转换：
if (memtable.current_size >= memtable.max_size ||          // MemTable达到大小限制
    system.memory_pressure() > threshold ||                 // 系统内存压力大
    wal.size() > wal_size_limit ||                         // WAL文件过大
    checkpoint_needed) {                                    // 需要检查点
    
    // 自动触发刷盘流程
    trigger_flush_to_sstable();
}
```

### 3.2 刷盘流程
```c
void trigger_flush_to_sstable() {
    // 1. 将当前活跃MemTable转为只读
    immutable_memtable = current_memtable;
    
    // 2. 创建新的活跃MemTable
    current_memtable = new MemTable();
    
    // 3. 异步将只读MemTable刷新到SSTable
    background_thread.submit([=]() {
        // 创建新的SSTable
        sstable_builder = new SSTableBuilder();
        
        // 将MemTable数据写入SSTable
        for (auto iter = immutable_memtable.iterator(); iter.valid(); iter.next()) {
            sstable_builder.add(iter.key(), iter.value());
        }
        
        // 完成SSTable构建
        new_sstable = sstable_builder.finish();
        
        // 更新元数据
        version_set.add_new_sstable(new_sstable);
        
        // 清理资源
        delete immutable_memtable;
    });
}
```

### 3.3 配置选项
```c
struct SSTableOptions {
    // MemTable相关配置
    size_t memtable_size;                    // MemTable大小限制
    double memtable_trigger_flush_ratio;     // 触发刷盘的使用率阈值
    
    // SSTable相关配置
    size_t sstable_size;                     // SSTable文件大小限制
    size_t max_file_size;                    // 最大文件大小
    
    // 后台任务配置
    int max_background_flushes;              // 最大后台刷盘线程数
    int max_background_compactions;          // 最大后台压缩线程数
    int level0_file_num_compaction_trigger;  // L0文件数触发压缩阈值
};
```

## 4. 手动控制接口

### 4.1 手动刷盘
```c
class StorageEngine {
    // 手动触发刷盘
    Status manual_flush() {
        if (active_memtable->empty()) {
            return Status::Nothing_To_Flush();
        }
        return trigger_flush_to_sstable();
    }
};
```

### 4.2 手动压缩
```c
class StorageEngine {
    // 手动触发压缩
    Status manual_compact(CompactionOptions options) {
        return trigger_compaction(options);
    }
};
```

## 5. 最佳实践

### 5.1 自动模式（推荐）
- 适用于生产环境
- 提供稳定性能
- 自动资源管理
- 减少人为干预

### 5.2 手动模式（特殊场景）
- 调试和测试
- 维护操作
- 性能调优
- 资源精确控制

### 5.3 推荐配置
```c
SSTableOptions options;
// 基本配置
options.memtable_size = 64 * 1024 * 1024;        // 64MB
options.memtable_trigger_flush_ratio = 0.75;      // 75%触发刷盘
options.sstable_size = 256 * 1024 * 1024;        // 256MB
options.max_file_size = 2 * 1024 * 1024 * 1024;  // 2GB

// 后台任务配置
options.level0_file_num_compaction_trigger = 4;   // L0文件数达到4个触发压缩
options.max_background_flushes = 2;               // 最多2个后台刷盘线程
options.max_background_compactions = 4;           // 最多4个后台压缩线程
```

## 6. 监控指标

### 6.1 关键指标
- 刷盘频率和耗时
- 压缩频率和耗时
- 文件数量和大小
- 内存使用情况
- 读写延迟

### 6.2 告警阈值
- 刷盘队列积压
- 压缩队列积压
- 文件数量过多
- 内存使用过高