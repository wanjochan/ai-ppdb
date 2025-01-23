# MemKV 任务计划

## 问题
- 需要实现一个纯内存的 KV 存储系统
- 需要兼容 Memcached 协议
- 暂不考虑持久化

## 分析

### 核心需求
1. 内存存储
   - 纯内存的 KV 存储结构
   - 高效的读写性能
   - 合理的内存管理策略

2. Memcached 协议兼容
   - 支持基本的 get/set 操作
   - 支持 delete 操作
   - 支持基本的统计信息

3. 网络服务
   - 复用现有的网络框架
   - 支持并发连接
   - 错误处理机制

### 技术方案

1. 代码组织
   - peer 组件：具体业务实现
     - peer_memkv.h：对外接口定义
     - peer_memkv.c：KV存储和Memcached协议实现
   
   - poly 组件：通用功能封装
     - poly_hashtable：通用哈希表实现，可被其他模块复用
     - poly_cmdline：复用现有命令行框架

2. 数据结构设计
   - KV 条目结构
     - key：字符串类型的键
     - value：二进制数据
     - value_size：值大小
     - flags：Memcached flags
     - exptime：过期时间

   - 存储引擎结构
     - hashtable：复用 poly_hashtable
     - stats：基础统计信息
     - config：运行时配置

## 执行计划

### 阶段一：基础框架
1. poly_hashtable 实现
   - 通用哈希表接口设计
   - 基本的 CRUD 操作
   - 支持自定义比较函数
   - 支持遍历操作

2. peer_memkv 基础结构
   - 初始化框架
   - 集成 poly_hashtable
   - 基本的 KV 操作封装

### 阶段二：Memcached 协议
1. 协议解析实现
   - 文本协议解析
   - 命令处理流程
   - 错误处理机制

2. 命令支持
   - get/set 实现
   - delete 实现
   - stats 实现

### 阶段三：完善功能
1. 性能优化
   - 内存管理优化
   - 并发处理优化
   - 过期处理机制

2. 测试验证
   - 单元测试
   - 并发测试
   - 性能测试

## 风险评估
1. 技术风险
   - 哈希表性能
   - 内存管理效率
   - 并发安全性

2. 兼容性风险
   - Memcached 协议兼容性
   - 不同客户端的支持

## 后续规划
1. 功能扩展
   - 更多 Memcached 命令支持
   - 简单持久化机制
   - 集群支持

2. 性能优化
   - 内存池优化
   - 并发优化
   - 网络性能优化

## 代码设计细节

### poly_hashtable 接口设计
1. 数据结构
   - hashtable_t：哈希表结构
     - 存储桶数组
     - 当前元素数量
     - 负载因子阈值
     - 哈希函数指针
     - 比较函数指针
   
2. 核心接口
   - hashtable_create：创建哈希表，支持配置初始大小和自定义函数
   - hashtable_destroy：销毁哈希表，清理所有资源
   - hashtable_insert：插入键值对，处理冲突和自动扩容
   - hashtable_find：查找键对应的值
   - hashtable_delete：删除键值对
   - hashtable_foreach：遍历哈希表的回调接口

3. 辅助功能
   - 自动扩容机制
   - 冲突解决（链地址法）
   - 迭代器支持

### peer_memkv 接口设计
1. 数据结构
   - memkv_t：KV存储主结构
     - hashtable：底层存储（使用 poly_hashtable）
     - stats：统计信息（命中率、总请求数等）
     - config：配置信息（最大内存、过期策略等）
   
   - memkv_item_t：存储项结构
     - key：字符串键
     - value：二进制数据
     - flags：Memcached 标志位
     - exptime：过期时间
     - size：数据大小
     - cas：版本号（可选）

2. 外部接口
   - memkv_init：初始化 KV 存储
   - memkv_shutdown：关闭 KV 存储
   - memkv_process_command：处理 Memcached 命令
   - memkv_get_stats：获取统计信息

3. 内部接口
   - memkv_get：获取键值
   - memkv_set：设置键值
   - memkv_delete：删除键值
   - memkv_expire：过期处理
   - memkv_parse_command：命令解析
   - memkv_format_response：响应格式化

### Memcached 协议实现
1. 文本协议命令
   ```
   get <key>
   set <key> <flags> <exptime> <bytes>
   delete <key>
   stats
   version
   quit
   ```

2. 响应格式
   ```
   VALUE <key> <flags> <bytes>
   <data>
   END

   STORED
   NOT_STORED
   DELETED
   NOT_FOUND
   ERROR
   CLIENT_ERROR
   SERVER_ERROR
   ```

3. 命令处理流程
   - 解析命令行
   - 验证参数
   - 执行操作
   - 格式化响应
   - 发送结果

### 内存管理策略
1. 内存分配
   - 小对象使用内存池
   - 大对象直接分配
   - 对齐处理

2. 过期处理
   - 惰性删除：访问时检查
   - 定期清理：后台任务
   - LRU 策略（可选）

3. 内存限制
   - 最大内存限制
   - 驱逐策略
   - 内存使用统计

### 并发控制
1. 基本策略
   - 读写锁分离
   - 分段锁设计
   - 无锁操作优化

2. 关键点保护
   - 哈希表扩容
   - 统计信息更新
   - 过期处理

### 错误处理
1. 错误类型
   - 系统错误（内存不足、网络错误）
   - 客户端错误（参数错误、格式错误）
   - 操作错误（键不存在、已存在）

2. 错误处理策略
   - 错误码定义
   - 日志记录
   - 客户端响应

### 网络实现设计
1. 整体架构
   - 采用 socket + select + 线程池模式
   - 复用 infra_net 和 infra_sync 基础设施
   - 参考 peer_rinetd 的框架设计

2. 关键结构
   - memkv_context：全局上下文
     - running：运行状态
     - store：KV存储实例
     - listener：监听socket
     - pool：线程池
     - config：配置信息

   - memkv_conn：客户端连接
     - socket：客户端socket
     - buffer：命令缓冲区
     - parse_state：协议解析状态
     - current_cmd：当前处理的命令

3. 工作流程
   - 主线程：监听连接
   - 工作线程：处理客户端命令
     1. 读取命令
     2. 解析协议
     3. 执行操作
     4. 返回结果

4. 线程安全
   - 连接级别：每个连接独立的缓冲区和状态
   - 存储级别：hashtable 的互斥访问
   - 统计级别：原子计数器

5. 性能考虑
   - 适当的线程池大小
   - 合理的缓冲区大小
   - 批量处理命令
   - 减少锁竞争

6. 错误处理
   - 网络错误处理
   - 协议错误处理
   - 资源清理

这种设计相比完全异步的方案有以下优势：
1. 实现简单直观
2. 便于调试和维护
3. 可靠性高
4. 充分复用现有代码

当然，如果后续需要更高的性能，我们也可以考虑迁移到 poly_async 架构，但目前这个方案应该足够满足需求。
