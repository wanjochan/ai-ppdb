# SSTable 设计文档

## 1. 概述

SSTable (Sorted String Table) 是 PPDB 的持久化存储格式，用于将内存中的数据以有序的方式存储到磁盘上。

### 1.1 设计目标
- 高效的顺序写入
- 快速的随机读取
- 支持范围扫描
- 节省存储空间
- 易于压缩和缓存

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

## 3. 索引设计

### 3.1 稀疏索引
```c
struct IndexBlock {
    uint32_t entry_count;   // 索引条目数量
    IndexEntry entries[];   // 索引条目数组
};

struct IndexEntry {
    uint16_t key_size;      // 键长度
    uint32_t offset;        // 数据块偏移
    uint32_t size;          // 数据块大小
    uint8_t  key[];         // 索引键
};
```

### 3.2 布隆过滤器
```c
struct BloomFilter {
    uint32_t bits_per_key;  // 每个键的位数
    uint32_t hash_count;    // 哈希函数数量
    uint32_t size;          // 过滤器大小
    uint8_t  data[];        // 过滤器数据
};
```

## 4. 压缩策略

### 4.1 块压缩
```c
enum CompressionType {
    NO_COMPRESSION = 0,
    SNAPPY = 1,
    LZ4 = 2,
    ZSTD = 3
};

struct CompressedBlock {
    uint32_t raw_size;      // 原始大小
    uint32_t compressed_size;// 压缩后大小
    uint8_t  type;          // 压缩类型
    uint8_t  data[];        // 压缩数据
};
```

### 4.2 压缩选项
```c
struct CompressionOptions {
    CompressionType type;   // 压缩类型
    int level;              // 压缩级别
    uint32_t min_size;      // 最小压缩大小
};
```

## 5. 读写操作

### 5.1 写入流程
```c
// 写入新的SSTable
SSTableBuilder* builder = sstable_builder_create(options);

// 添加数据
while (has_next()) {
    KeyValue kv = get_next();
    sstable_builder_add(builder, kv.key, kv.value);
}

// 完成构建
sstable_builder_finish(builder);
```

### 5.2 读取流程
```c
// 打开SSTable
SSTable* table = sstable_open(filename);

// 查找键
Value* value = sstable_get(table, key);

// 范围查询
Iterator* iter = sstable_range(table, start_key, end_key);
while (iterator_valid(iter)) {
    // 处理数据
    iterator_next(iter);
}
```

## 6. 性能优化

### 6.1 缓存策略
```c
struct CacheOptions {
    size_t block_cache_size;    // 块缓存大小
    size_t filter_cache_size;   // 过滤器缓存大小
    bool   pin_l0_filter_and_index; // 是否固定L0层
};
```

### 6.2 预读策略
```c
struct ReadOptions {
    bool  verify_checksums;     // 是否验证校验和
    bool  fill_cache;          // 是否填充缓存
    size_t readahead_size;     // 预读大小
};
```

## 7. 文件管理

### 7.1 文件命名
```
[level]_[sequence].sst
示例：0_1001.sst
```

### 7.2 版本控制
```c
struct Version {
    uint32_t number;           // 版本号
    std::vector<FileMetaData> files[kNumLevels];
};

struct FileMetaData {
    uint64_t number;           // 文件号
    uint64_t file_size;        // 文件大小
    InternalKey smallest;      // 最小键
    InternalKey largest;       // 最大键
};
```

## 8. 错误处理

### 8.1 错误类型
```c
enum SSTableError {
    SSTABLE_OK = 0,
    SSTABLE_CORRUPTION,
    SSTABLE_IO_ERROR,
    SSTABLE_COMPRESSION_ERROR,
    SSTABLE_INVALID_ARGUMENT
};
```

### 8.2 恢复机制
```c
struct RecoveryOptions {
    bool paranoid_checks;      // 严格检查
    bool ignore_corruption;    // 忽略损坏
    bool force_consistency;    // 强制一致性
};
```
