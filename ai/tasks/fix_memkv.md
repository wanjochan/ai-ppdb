# 改进 peer_memkv 实现

## 问题分析

1. 当前问题
   - `peer_memkv` 实现过于复杂
   - 自己实现存储引擎导致功能不稳定
   - 代码维护成本高

2. 改进方向
   - 基于 `poly_db` (SQLite) 重新实现 KV 存储
   - 复用 SQLite 的事务和并发控制
   - 简化代码结构

## 实现方案

1. 数据结构设计
```sql
CREATE TABLE IF NOT EXISTS kv_store (
    key TEXT PRIMARY KEY,  -- KV 存储的键
    value BLOB,           -- 值(二进制)
    flags INTEGER,        -- memcached 协议的 flags
    expiry INTEGER        -- 过期时间戳
);

CREATE INDEX IF NOT EXISTS idx_expiry ON kv_store(expiry);  -- 用于过期清理
```

2. 核心功能
   - 使用 `poly_db` 替换 `poly_memkv`
   - 实现 KV 基本操作:
     * SET: INSERT OR REPLACE INTO kv_store
     * GET: SELECT value, flags FROM kv_store WHERE key = ? AND (expiry = 0 OR expiry > ?)
     * DELETE: DELETE FROM kv_store WHERE key = ?
     * INCR/DECR: 使用 SQLite 事务保证原子性
   - 过期处理:
     * 惰性删除: GET 时检查
     * 定期清理: 后台任务删除过期键

3. 改造步骤
   a. 修改 peer_memkv.c:
      - 移除自定义存储引擎相关代码
      - 添加 SQLite 表初始化
      - 实现基于 SQL 的 KV 操作
   
   b. 连接处理:
      - 复用 peer_sqlite3 的连接池
      - 使用 WAL 模式提升并发性能
      - 统一错误处理

   c. 协议兼容:
      - 保持 memcached 文本协议
      - 支持二进制协议(可选)

## 执行计划

1. 第一阶段: 基础改造
   - [ ] 创建数据库表结构
   - [ ] 实现基本 KV 操作
   - [ ] 添加过期处理

2. 第二阶段: 性能优化
   - [ ] 配置 WAL 模式
   - [ ] 优化并发处理
   - [ ] 添加性能监控

3. 第三阶段: 功能完善
   - [ ] 实现 INCR/DECR
   - [ ] 添加批量操作
   - [ ] 完善错误处理

## 注意事项

1. 数据兼容性
   - 需要考虑已有数据的迁移
   - 保持键值对格式兼容

2. 性能考虑
   - 使用 prepared statements
   - 合理设置 cache_size
   - 批量操作使用事务

3. 可靠性
   - 添加完整的错误处理
   - 实现数据一致性检查
   - 添加监控和日志

## 进度跟踪

- [ ] 完成基础设计
- [ ] 实现核心功能
- [ ] 测试和优化
- [ ] 文档更新 