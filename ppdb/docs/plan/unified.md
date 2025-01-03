# PPDB统一存储层次结构计划

## 1. 架构概览

### 1.1 层次结构
```
KVStore层 (kvstore_t)
    ↓
容器层 (container_t)
    ↓
存储层 (storage_t)
    ↓
基础层 (base_t)
``` 