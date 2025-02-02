# SQLite3服务段错误分析

## 问题描述
在SQLite3服务关闭连接时出现段错误。从日志可以看到:
1. 服务正常启动并监听5433端口
2. 成功接受来自127.0.0.1:49737的连接
3. 成功打开内存数据库并建立连接
4. 客户端连接后立即断开
5. 在关闭连接时发生段错误

## 代码分析
经过检查代码，发现以下几个关键点：

1. 连接创建流程 (sqlite3_conn_create):
   - 分配连接结构体
   - 设置客户端socket
   - 打开数据库连接
   - 返回连接对象

2. 连接销毁流程 (sqlite3_conn_destroy):
   - 检查连接对象是否为NULL
   - 关闭数据库连接
   - 设置client为NULL
   - 释放连接结构体

3. 请求处理流程 (handle_request_wrapper):
   - 客户端断开连接时
   - 设置conn->client = NULL
   - 调用sqlite3_conn_destroy(conn)

## 问题定位
通过深入分析代码，发现了几个关键问题：

1. 数据库连接关闭问题：
   - sqlite3_conn_destroy 直接调用 poly_db_close
   - poly_db_close 和 sqlite_close 都会释放 db 结构体
   - 导致重复释放内存

2. 资源清理顺序：
   - 没有正确处理未完成的预处理语句
   - 没有使用 sqlite3_close_v2 作为备选关闭方法
   - 缺少错误处理和日志记录

3. 内存管理：
   - handler_args 的内存管理不正确
   - 连接结构体中缺少状态标记
   - 资源清理的顺序不明确

## 修复方案
已实施以下修改：

1. 修改 poly_db_close 函数：
```c
infra_error_t poly_db_close(poly_db_t* db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    if (db->close) {
        db->close(db);  // close 函数会释放 db
        return INFRA_OK;
    }
    infra_free(db);  // 只有在没有 close 函数时才直接释放
    return INFRA_OK;
}
```

2. 修改 sqlite3_conn_destroy 函数：
```c
static void sqlite3_conn_destroy(sqlite3_conn_t* conn) {
    if (!conn) {
        INFRA_LOG_ERROR("Attempting to destroy NULL connection");
        return;
    }

    // 标记连接正在关闭，防止重复清理
    if (conn->is_closing) {
        INFRA_LOG_DEBUG("Connection already being destroyed");
        return;
    }
    conn->is_closing = true;

    INFRA_LOG_DEBUG("Destroying connection: client=%p, db=%p", conn->client, conn->db);

    // 先关闭数据库连接
    if (conn->db) {
        INFRA_LOG_DEBUG("Closing database connection");
        // 先等待所有未完成的语句完成
        sqlite3_stmt* stmt = NULL;
        while ((stmt = sqlite3_next_stmt(conn->db, NULL)) != NULL) {
            sqlite3_finalize(stmt);
        }
        
        // 关闭数据库连接
        int rc = sqlite3_close(conn->db);
        if (rc != SQLITE_OK) {
            INFRA_LOG_ERROR("Failed to close SQLite database: %s", sqlite3_errmsg(conn->db));
            // 强制关闭
            rc = sqlite3_close_v2(conn->db);
            if (rc != SQLITE_OK) {
                INFRA_LOG_ERROR("Failed to force close SQLite database: %s", sqlite3_errmsg(conn->db));
            }
        }
        conn->db = NULL;
    }

    // 注意：不要关闭 client socket，它由 poly_poll 管理
    if (conn->client) {
        INFRA_LOG_DEBUG("Clearing client socket reference");
        conn->client = NULL;
    }

    // 最后释放连接结构体
    INFRA_LOG_DEBUG("Freeing connection structure");
    infra_free(conn);
}
```

3. 改进错误处理：
   - 添加更多的日志记录
   - 在关键点添加错误检查
   - 确保资源释放的完整性

## 验证步骤
1. 重新编译服务
2. 启动服务
3. 使用测试客户端连接并立即断开
4. 观察日志，确认没有段错误
5. 重复以上步骤多次，确保稳定性

## 后续建议
1. 考虑添加连接状态机，更好地管理连接的生命周期
2. 添加更多的防御性检查，避免资源泄露
3. 考虑使用引用计数来管理共享资源
4. 在测试套件中添加连接生命周期的测试用例
5. 添加内存泄漏检测工具的支持
6. 考虑添加数据库连接池来提高性能和稳定性
7. 添加更多的单元测试，特别是针对边缘情况的测试 
6. 考虑添加数据库连接池来提高性能和稳定性 