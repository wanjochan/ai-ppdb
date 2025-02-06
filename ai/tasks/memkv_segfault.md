# memkv segfault 分析

## 问题描述
在运行 memkv 服务时,当有新连接到达时出现 segmentation fault。具体日志:

```
[2025-02-06 13:07:08] INFO memkv_apply_config(): Applied configuration - port: 11211, engine: default
[2025-02-06 13:07:08] INFO poly_poll_add_listener(): Added listener on 0.0.0.0:11211
[2025-02-06 13:07:11] INFO poly_poll_start(): New connection from 127.0.0.1:59270 on socket 0
[2025-02-06 13:07:11] INFO handle_connection(): New client connection from 127.0.0.1:59270
Segmentation fault: 11
```

## 分析过程

### 1. 代码流程分析
1. memkv 服务启动并监听 11211 端口
2. 有新连接到达,触发 handle_connection()
3. handle_connection() 中创建了连接上下文
4. 在某个步骤发生了段错误

### 2. 可能的问题点
1. handle_connection() 中内存分配/初始化问题
2. poly_poll 相关的内存管理问题
3. 数据库连接初始化问题

### 3. 深入分析
#### 3.1 handle_connection() 函数分析
1. 函数流程:
   - 获取客户端地址
   - 分配连接上下文
   - 初始化数据库连接
   - 创建表
   - 分配接收缓冲区
   - 设置socket选项

2. 关键点:
   - 内存分配检查是否完整
   - 数据库连接初始化是否正确
   - socket 选项设置是否有问题

#### 3.2 poly_poll 相关分析
1. poly_poll_context_t 结构:
   - running 标志
   - 线程池
   - 监听 socket 数组
   - poll 事件数组
   - 监听器配置数组
   - 连接处理回调

2. 关键点:
   - 线程池是否正确初始化
   - socket 数组管理是否有问题
   - 事件处理是否有竞态条件

## 已发现和修复的问题

### 1. 数据库配置初始化问题
1. g_memkv_state.engine 可能为 NULL 导致 strcmp 崩溃
   - 修复: 添加 NULL 检查
   ```c
   db_config.type = (g_memkv_state.engine && strcmp(g_memkv_state.engine, "duckdb") == 0) ? 
                    POLY_DB_TYPE_DUCKDB : POLY_DB_TYPE_SQLITE;
   ```

2. 数据库配置中的指针可能悬空
   - 问题: db_config.plugin_path 和 db_config.url 都直接指向 g_memkv_state.plugin
   - 修复: 使用 strdup 创建独立副本
   ```c
   const char* plugin_path = g_memkv_state.plugin ? g_memkv_state.plugin : ":memory:";
   db_config.url = strdup(plugin_path);
   db_config.plugin_path = db_config.url;
   ```
   - 确保在适当时机释放内存

### 2. Socket 选项设置问题
1. 设置 TCP_NODELAY 失败时没有清理资源
   - 修复: 添加资源清理
   ```c
   if (setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
       INFRA_LOG_ERROR("Failed to set TCP_NODELAY");
       poly_db_close(conn->store);
       infra_free(conn->rx_buf);
       infra_free(conn);
       infra_net_close(client);
       return;
   }
   ```

2. 设置非阻塞模式失败时没有清理资源
   - 修复: 添加资源清理和错误处理
   ```c
   if (fcntl(client, F_SETFL, flags | O_NONBLOCK) < 0) {
       INFRA_LOG_ERROR("Failed to set non-blocking mode");
       poly_db_close(conn->store);
       infra_free(conn->rx_buf);
       infra_free(conn);
       infra_net_close(client);
       return;
   }
   ```

### 3. 连接处理问题
1. handle_request() 中没有完整的 socket 有效性检查
   - 修复: 添加更严格的检查
   ```c
   if (!handler_args->client || handler_args->client < 0) {
       INFRA_LOG_ERROR("Invalid client socket: %d", handler_args->client);
       return;
   }
   ```

2. 连接状态检查不完整
   - 修复: 添加更多检查条件
   ```c
   if (!conn || !conn->store || !conn->rx_buf || conn->sock <= 0) {
       INFRA_LOG_ERROR("Invalid connection state");
       if (conn) {
           if (conn->rx_buf) infra_free(conn->rx_buf);
           if (conn->store) poly_db_close(conn->store);
           if (conn->sock > 0) infra_net_close(conn->sock);
           infra_free(conn);
           handler_args->user_data = NULL;
       }
       return;
   }
   ```

3. 资源清理不完整
   - 修复: 添加 socket 有效性检查和状态重置
   ```c
   if (conn->sock > 0) {
       infra_net_close(conn->sock);
       conn->sock = -1;
   }
   ```

### 4. 命令处理问题
1. 接收数据前没有检查缓冲区和 socket 有效性
   ```c
   if (!conn->rx_buf) {
       INFRA_LOG_ERROR("Invalid receive buffer");
       break;
   }
   if (conn->sock <= 0) {
       INFRA_LOG_ERROR("Invalid socket");
       break;
   }
   ```

2. SET 命令参数验证不完整
   ```c
   char* endptr = NULL;
   long bytes = strtol(bytes_str, &endptr, 10);
   if (*endptr != '\0' || bytes < 0) {
       INFRA_LOG_ERROR("Invalid bytes value: %s", bytes_str);
       if (conn->sock > 0) {
           err = send_all(conn->sock, "CLIENT_ERROR invalid bytes value\r\n", 32);
           // ...
       }
       continue;
   }
   ```

3. INCR/DECR 命令参数验证不完整
   ```c
   char* endptr = NULL;
   long value = strtol(value_str, &endptr, 10);
   if (*endptr != '\0' || value < 0) {
       INFRA_LOG_ERROR("Invalid value: %s", value_str);
       if (conn->sock > 0) {
           err = send_all(conn->sock, "CLIENT_ERROR invalid value\r\n", 27);
           // ...
       }
       continue;
   }
   ```

4. 发送错误响应时没有检查 socket 有效性
   ```c
   if (conn->sock > 0) {
       err = send_all(conn->sock, "ERROR\r\n", 7);
       // ...
   }
   ```

### 6. GET 命令处理问题
1. 参数验证不完整
   - 修复: 添加 socket 有效性检查
   ```c
   if (!conn || !conn->store || !key || conn->sock <= 0) {
       INFRA_LOG_ERROR("Invalid parameters in handle_get");
       if (conn && conn->sock > 0) {
           infra_net_send(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL);
       }
       return;
   }
   ```

2. 响应头格式化没有边界检查
   - 修复: 添加格式化结果检查
   ```c
   if (header_len < 0 || header_len >= (int)sizeof(header)) {
       INFRA_LOG_ERROR("Failed to format header: key too long");
       free(value);
       if (conn->sock > 0) {
           infra_net_send(conn->sock, "SERVER_ERROR header too long\r\n", 28, NULL);
       }
       conn->should_close = true;
       return;
   }
   ```

3. 发送数据时没有检查 socket 有效性
   - 修复: 在每次发送前检查 socket
   ```c
   if (conn->sock <= 0) {
       INFRA_LOG_ERROR("Invalid socket when sending header");
       free(value);
       return;
   }
   ```

4. 错误处理不完整
   - 修复: 在发送错误响应前检查 socket 有效性
   ```c
   if (conn->sock > 0) {
       err = send_all(conn->sock, "END\r\n", 5);
       if (err != INFRA_OK) {
           INFRA_LOG_ERROR("Failed to send END response: err=%d", err);
           conn->should_close = true;
       }
   }
   ```

### 7. DELETE 命令处理问题
1. 参数验证不完整
   - 修复: 添加 socket 有效性检查
   ```c
   if (!conn || !conn->store || !key || conn->sock <= 0) {
       INFRA_LOG_ERROR("Invalid parameters in handle_delete");
       if (!noreply && conn && conn->sock > 0) {
           infra_net_send(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL);
       }
       return;
   }
   ```

2. key 长度没有限制
   - 修复: 添加 key 长度检查
   ```c
   size_t key_len = strlen(key);
   if (key_len == 0 || key_len > 250) {  // 250 是一个合理的限制
       INFRA_LOG_ERROR("Invalid key length: %zu", key_len);
       if (!noreply && conn->sock > 0) {
           infra_net_send(conn->sock, "CLIENT_ERROR invalid key length\r\n", 31, NULL);
       }
       return;
   }
   ```

3. 错误处理不完整
   - 修复: 在发送错误响应前检查 socket 有效性
   ```c
   if (!noreply && conn->sock > 0) {
       if (err == INFRA_OK) {
           err = send_all(conn->sock, "DELETED\r\n", 9);
           if (err != INFRA_OK) {
               INFRA_LOG_ERROR("Failed to send DELETED response: %d", err);
               conn->should_close = true;
           }
       } else {
           err = send_all(conn->sock, "NOT_FOUND\r\n", 11);
           if (err != INFRA_OK) {
               INFRA_LOG_ERROR("Failed to send NOT_FOUND response: %d", err);
               conn->should_close = true;
           }
       }
   }
   ```

4. 性能优化
   - 修复: 缓存 key 长度避免重复计算
   ```c
   size_t key_len = strlen(key);
   err = poly_db_bind_text(stmt, 1, key, key_len);
   ```

## 从 peer_sqlite3.c 学到的最佳实践

### 1. 连接管理
1. 创建连接时进行完整的资源初始化检查
   ```c
   if (!conn) {
       INFRA_LOG_ERROR("Failed to allocate connection");
       return NULL;
   }
   ```

2. 销毁连接时使用标志避免重复销毁
   ```c
   if (conn->is_closing) {
       INFRA_LOG_DEBUG("Connection already being destroyed");
       return;
   }
   conn->is_closing = true;
   ```

3. 验证清理完成情况
   ```c
   if (conn->db != NULL) {
       INFRA_LOG_ERROR("Database connection not properly cleaned up");
   }
   ```

### 2. 数据库优化
1. 使用 WAL 模式提高并发性能
   ```c
   err = poly_db_exec(conn->db, "PRAGMA journal_mode=WAL;");
   ```

2. 设置适当的缓存大小
   ```c
   err = poly_db_exec(conn->db, "PRAGMA cache_size=2000;");  // ~8MB cache
   ```

3. 优化写入性能
   ```c
   err = poly_db_exec(conn->db, "PRAGMA synchronous=NORMAL;");
   ```

### 3. 错误处理
1. 详细的错误日志
   ```c
   INFRA_LOG_ERROR("Failed to create poll: %d", err);
   ```

2. 资源清理链
   ```c
   if (err != INFRA_OK) {
       poly_db_close(conn->db);
       infra_free(conn);
       return NULL;
   }
   ```

3. 状态检查
   ```c
   if (!state || !state->running) {
       return INFRA_ERROR_INVALID_STATE;
   }
   ```

### 4. 性能优化
1. 非阻塞 IO
   ```c
   err = infra_net_set_nonblock(client, true);
   ```

2. 超时设置
   ```c
   err = poly_db_exec(conn->db, "PRAGMA busy_timeout=5000;");
   ```

3. 缓冲区管理
   ```c
   #define SQLITE3_MAX_SQL_LEN 4096
   char buffer[SQLITE3_MAX_SQL_LEN];
   ```

### 5. 安全性
1. 字符串拷贝使用安全函数
   ```c
   strncpy(state->host, host, SQLITE3_MAX_HOST_LEN - 1);
   state->host[SQLITE3_MAX_HOST_LEN - 1] = '\0';
   ```

2. 参数验证
   ```c
   if (!cmd || !response || size == 0) {
       return INFRA_ERROR_INVALID_PARAM;
   }
   ```

3. 资源限制
   ```c
   #define SQLITE3_MAX_CONNECTIONS 128
   ```

## 需要改进的地方
1. memkv 服务也应该添加连接状态标志
2. 实现更完善的资源清理验证
3. 添加数据库性能优化配置
4. 改进错误处理和日志记录
5. 实现更好的缓冲区管理
6. 添加更多的安全检查

## 下一步计划
1. 实现连接状态管理改进
2. 添加数据库优化配置
3. 完善错误处理机制
4. 改进资源管理
5. 添加性能监控
6. 实现更好的安全机制 