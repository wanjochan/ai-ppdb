#include "internal/infra/infra_core.h"
#include "internal/peer/peer_memkv_cmd.h"
#include <stdarg.h>

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static infra_error_t handle_get(memkv_conn_t* conn);
static infra_error_t handle_set(memkv_conn_t* conn);
static infra_error_t handle_add(memkv_conn_t* conn);
static infra_error_t handle_replace(memkv_conn_t* conn);
static infra_error_t handle_append(memkv_conn_t* conn);
static infra_error_t handle_prepend(memkv_conn_t* conn);
static infra_error_t handle_cas(memkv_conn_t* conn);
static infra_error_t handle_delete(memkv_conn_t* conn);
static infra_error_t handle_incr(memkv_conn_t* conn);
static infra_error_t handle_decr(memkv_conn_t* conn);
static infra_error_t handle_touch(memkv_conn_t* conn);
static infra_error_t handle_gat(memkv_conn_t* conn);
static infra_error_t handle_flush_all(memkv_conn_t* conn);
static infra_error_t handle_stats(memkv_conn_t* conn);
static infra_error_t handle_version(memkv_conn_t* conn);
static infra_error_t handle_quit(memkv_conn_t* conn);

// 命令处理器表
static const memkv_cmd_handler_t cmd_handlers[] = {
    // 基础命令 - 已实现
    {"get",     CMD_GET,     handle_get,     2, 2, false},
    {"set",     CMD_SET,     handle_set,     5, 5, true},
    {"delete",  CMD_DELETE,  handle_delete,  2, 2, false},
    {"stats",   CMD_STATS,   handle_stats,   1, 1, false},
    {"version", CMD_VERSION, handle_version, 1, 1, false},
    {"quit",    CMD_QUIT,    handle_quit,    1, 1, false},
    
    // 存储扩展命令 - 第二阶段
    {"add",     CMD_ADD,     handle_add,     5, 5, true},
    {"replace", CMD_REPLACE, handle_replace, 5, 5, true},
    {"append",  CMD_APPEND,  handle_append,  5, 5, true},
    {"prepend", CMD_PREPEND, handle_prepend, 5, 5, true},
    {"cas",     CMD_CAS,     handle_cas,     6, 6, true},
    
    // 功能扩展命令 - 第三阶段
    {"incr",     CMD_INCR,     handle_incr,     3, 3, false},
    {"decr",     CMD_DECR,     handle_decr,     3, 3, false},
    {"touch",    CMD_TOUCH,    handle_touch,    3, 3, false},
    {"gat",      CMD_GAT,      handle_gat,      3, -1, false},
    {"flush_all",CMD_FLUSH,    handle_flush_all,1, 2, false},
    
    // 结束标记
    {NULL,      CMD_UNKNOWN, NULL,           0, 0, false}
};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 分割命令行
static int split_command(char* line, char* tokens[], int max_tokens) {
    int count = 0;
    char* token = strtok(line, " \t\r\n");
    
    while (token && count < max_tokens) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    
    return count;
}

// 查找命令处理器
static const memkv_cmd_handler_t* find_handler(const char* cmd) {
    for (const memkv_cmd_handler_t* h = cmd_handlers; h->name; h++) {
        if (strcasecmp(cmd, h->name) == 0) {
            return h;
        }
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// Command Implementation
//-----------------------------------------------------------------------------

infra_error_t memkv_parse_command(memkv_conn_t* conn) {
    // 查找命令结束符
    char* eol = strstr(conn->buffer, "\r\n");
    if (!eol) {
        if (conn->buffer_used >= MEMKV_MAX_CMD_LEN) {
            return INFRA_ERROR_BUFFER_FULL;
        }
        return INFRA_ERROR_WOULD_BLOCK;
    }

    // 分割命令
    *eol = '\0';
    char* tokens[MEMKV_MAX_TOKENS];
    int token_count = split_command(conn->buffer, tokens, MEMKV_MAX_TOKENS);

    if (token_count == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 查找命令处理器
    const memkv_cmd_handler_t* handler = find_handler(tokens[0]);
    if (!handler) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // 检查参数数量
    if (token_count < handler->min_tokens || token_count > handler->max_tokens) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 设置命令类型
    conn->current_cmd.type = handler->type;

    // 解析命令参数
    switch (handler->type) {
        case CMD_GET:
            conn->current_cmd.key = strdup(tokens[1]);
            break;

        case CMD_SET:
            conn->current_cmd.key = strdup(tokens[1]);
            conn->current_cmd.flags = strtoul(tokens[2], NULL, 10);
            conn->current_cmd.exptime = strtol(tokens[3], NULL, 10);
            conn->current_cmd.bytes = strtoul(tokens[4], NULL, 10);
            
            if (conn->current_cmd.bytes > MEMKV_MAX_VALUE_SIZE) {
                return INFRA_ERROR_INVALID_PARAM;
            }
            
            conn->data_remaining = conn->current_cmd.bytes + 2; // +2 for \r\n
            conn->state = PARSE_STATE_DATA;
            break;

        case CMD_DELETE:
            conn->current_cmd.key = strdup(tokens[1]);
            break;

        default:
            break;
    }

    // 如果不需要额外数据，标记为完成
    if (!handler->need_data) {
        conn->state = PARSE_STATE_COMPLETE;
    }

    // 移动缓冲区
    size_t cmd_len = (eol - conn->buffer) + 2;
    memmove(conn->buffer, conn->buffer + cmd_len, 
            conn->buffer_used - cmd_len);
    conn->buffer_used -= cmd_len;

    return INFRA_OK;
}

infra_error_t memkv_execute_command(memkv_conn_t* conn) {
    const memkv_cmd_handler_t* handler = find_handler(
        cmd_handlers[conn->current_cmd.type].name);
    
    if (!handler || !handler->fn) {
        return INFRA_ERROR_NOT_FOUND;
    }

    return handler->fn(conn);
}

infra_error_t memkv_send_response(memkv_conn_t* conn, const char* fmt, ...) {
    char buffer[MEMKV_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len < 0 || len >= sizeof(buffer)) {
        return INFRA_ERROR_BUFFER_FULL;
    }

    size_t bytes_sent = 0;
    return infra_net_send(conn->socket, buffer, len, &bytes_sent);
}

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static infra_error_t handle_get(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    bool hit = false;

    infra_mutex_lock(&g_context.store_mutex);
    infra_error_t err = poly_hashtable_get(g_context.store, 
        conn->current_cmd.key, (void**)&item);
    
    if (err == INFRA_OK && !is_item_expired(item)) {
        hit = true;
        err = memkv_send_response(conn, 
            "VALUE %s %u %zu %lu\r\n", 
            item->key, item->flags, item->value_size, item->cas);
        
        if (err == INFRA_OK) {
            size_t bytes_sent = 0;
            err = infra_net_send(conn->socket, item->value, 
                item->value_size, &bytes_sent);
            
            if (err == INFRA_OK) {
                err = memkv_send_response(conn, "\r\n");
            }
        }
    }
    infra_mutex_unlock(&g_context.store_mutex);

    update_stats_get(hit);

    if (err == INFRA_OK) {
        err = memkv_send_response(conn, "END\r\n");
    }

    return err;
}

static infra_error_t handle_set(memkv_conn_t* conn) {
    memkv_item_t* item = create_item(
        conn->current_cmd.key,
        conn->current_cmd.data,
        conn->current_cmd.bytes,
        conn->current_cmd.flags,
        conn->current_cmd.exptime
    );

    if (!item) {
        return INFRA_ERROR_NO_MEMORY;
    }

    infra_mutex_lock(&g_context.store_mutex);
    
    // 先删除旧值
    memkv_item_t* old_item = NULL;
    if (poly_hashtable_get(g_context.store, item->key, 
        (void**)&old_item) == INFRA_OK) {
        update_stats_delete(old_item->value_size);
    }

    // 存储新值
    infra_error_t err = poly_hashtable_put(g_context.store, 
        item->key, item);
    
    infra_mutex_unlock(&g_context.store_mutex);

    if (err == INFRA_OK) {
        update_stats_set(item->value_size);
        return memkv_send_response(conn, "STORED\r\n");
    } else {
        destroy_item(item);
        return memkv_send_response(conn, "NOT_STORED\r\n");
    }
}

static infra_error_t handle_delete(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    
    memkv_item_t* item = NULL;
    infra_error_t err = poly_hashtable_get(g_context.store, 
        conn->current_cmd.key, (void**)&item);
    
    if (err == INFRA_OK && !is_item_expired(item)) {
        update_stats_delete(item->value_size);
        err = poly_hashtable_remove(g_context.store, item->key);
        destroy_item(item);
    }
    
    infra_mutex_unlock(&g_context.store_mutex);

    return memkv_send_response(conn, 
        err == INFRA_OK ? "DELETED\r\n" : "NOT_FOUND\r\n");
}

static infra_error_t handle_stats(memkv_conn_t* conn) {
    const memkv_stats_t* stats = memkv_get_stats();
    
    return memkv_send_response(conn,
        "STAT cmd_get %lu\r\n"
        "STAT cmd_set %lu\r\n"
        "STAT cmd_delete %lu\r\n"
        "STAT get_hits %lu\r\n"
        "STAT get_misses %lu\r\n"
        "STAT curr_items %lu\r\n"
        "STAT total_items %lu\r\n"
        "STAT bytes %lu\r\n"
        "END\r\n",
        stats->cmd_get,
        stats->cmd_set,
        stats->cmd_delete,
        stats->hits,
        stats->misses,
        stats->curr_items,
        stats->total_items,
        stats->bytes
    );
}

static infra_error_t handle_version(memkv_conn_t* conn) {
    return memkv_send_response(conn, "VERSION %s\r\n", MEMKV_VERSION);
}

static infra_error_t handle_quit(memkv_conn_t* conn) {
    return INFRA_ERROR_CLOSED;
}

//-----------------------------------------------------------------------------
// Storage Commands Implementation - Phase 2
//-----------------------------------------------------------------------------

static infra_error_t handle_add(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 检查键是否已存在
    void* existing = NULL;
    if (poly_hashtable_get(g_context.store, conn->current_cmd.key, &existing) == INFRA_OK) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "NOT_STORED\\r\\n");
    }
    
    // 创建新项
    memkv_item_t* item = create_item(
        conn->current_cmd.key,
        conn->current_cmd.data,
        conn->current_cmd.bytes,
        conn->current_cmd.flags,
        conn->current_cmd.exptime
    );
    
    if (!item) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "SERVER_ERROR out of memory\\r\\n");
    }
    
    // 存储
    infra_error_t err = poly_hashtable_put(g_context.store, item->key, item);
    infra_mutex_unlock(&g_context.store_mutex);
    
    if (err == INFRA_OK) {
        update_stats_set(item->value_size);
        return memkv_send_response(conn, "STORED\\r\\n");
    } else {
        destroy_item(item);
        return memkv_send_response(conn, "NOT_STORED\\r\\n");
    }
}

static infra_error_t handle_replace(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 检查键是否存在
    memkv_item_t* old_item = NULL;
    if (poly_hashtable_get(g_context.store, conn->current_cmd.key, 
        (void**)&old_item) != INFRA_OK) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "NOT_STORED\\r\\n");
    }
    
    // 创建新项
    memkv_item_t* item = create_item(
        conn->current_cmd.key,
        conn->current_cmd.data,
        conn->current_cmd.bytes,
        conn->current_cmd.flags,
        conn->current_cmd.exptime
    );
    
    if (!item) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "SERVER_ERROR out of memory\\r\\n");
    }
    
    // 更新统计信息
    update_stats_delete(old_item->value_size);
    
    // 存储新项
    infra_error_t err = poly_hashtable_put(g_context.store, item->key, item);
    infra_mutex_unlock(&g_context.store_mutex);
    
    if (err == INFRA_OK) {
        update_stats_set(item->value_size);
        destroy_item(old_item);
        return memkv_send_response(conn, "STORED\\r\\n");
    } else {
        destroy_item(item);
        return memkv_send_response(conn, "NOT_STORED\\r\\n");
    }
}

static infra_error_t handle_append_prepend(memkv_conn_t* conn, bool append) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 获取现有项
    memkv_item_t* old_item = NULL;
    if (poly_hashtable_get(g_context.store, conn->current_cmd.key, 
        (void**)&old_item) != INFRA_OK) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "NOT_STORED\\r\\n");
    }
    
    // 计算新大小
    size_t new_size = old_item->value_size + conn->current_cmd.bytes;
    
    // 创建新缓冲区
    void* new_value = malloc(new_size);
    if (!new_value) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "SERVER_ERROR out of memory\\r\\n");
    }
    
    // 复制数据
    if (append) {
        memcpy(new_value, old_item->value, old_item->value_size);
        memcpy((char*)new_value + old_item->value_size, 
            conn->current_cmd.data, conn->current_cmd.bytes);
    } else {
        memcpy(new_value, conn->current_cmd.data, conn->current_cmd.bytes);
        memcpy((char*)new_value + conn->current_cmd.bytes, 
            old_item->value, old_item->value_size);
    }
    
    // 创建新项，保持原有的标志和过期时间
    memkv_item_t* item = create_item(
        conn->current_cmd.key,
        new_value,
        new_size,
        old_item->flags,
        old_item->exptime ? 
            old_item->exptime - time(NULL) : 0
    );
    
    free(new_value);
    
    if (!item) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "SERVER_ERROR out of memory\\r\\n");
    }
    
    // 更新统计信息
    update_stats_delete(old_item->value_size);
    
    // 存储新项
    infra_error_t err = poly_hashtable_put(g_context.store, item->key, item);
    infra_mutex_unlock(&g_context.store_mutex);
    
    if (err == INFRA_OK) {
        update_stats_set(item->value_size);
        destroy_item(old_item);
        return memkv_send_response(conn, "STORED\\r\\n");
    } else {
        destroy_item(item);
        return memkv_send_response(conn, "NOT_STORED\\r\\n");
    }
}

static infra_error_t handle_append(memkv_conn_t* conn) {
    return handle_append_prepend(conn, true);
}

static infra_error_t handle_prepend(memkv_conn_t* conn) {
    return handle_append_prepend(conn, false);
}

static infra_error_t handle_cas(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 获取现有项
    memkv_item_t* old_item = NULL;
    if (poly_hashtable_get(g_context.store, conn->current_cmd.key, 
        (void**)&old_item) != INFRA_OK) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "NOT_FOUND\\r\\n");
    }
    
    // 检查 CAS 值
    if (old_item->cas != conn->current_cmd.cas) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "EXISTS\\r\\n");
    }
    
    // 创建新项
    memkv_item_t* item = create_item(
        conn->current_cmd.key,
        conn->current_cmd.data,
        conn->current_cmd.bytes,
        conn->current_cmd.flags,
        conn->current_cmd.exptime
    );
    
    if (!item) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "SERVER_ERROR out of memory\\r\\n");
    }
    
    // 更新统计信息
    update_stats_delete(old_item->value_size);
    
    // 存储新项
    infra_error_t err = poly_hashtable_put(g_context.store, item->key, item);
    infra_mutex_unlock(&g_context.store_mutex);
    
    if (err == INFRA_OK) {
        update_stats_set(item->value_size);
        destroy_item(old_item);
        return memkv_send_response(conn, "STORED\\r\\n");
    } else {
        destroy_item(item);
        return memkv_send_response(conn, "NOT_STORED\\r\\n");
    }
}

//-----------------------------------------------------------------------------
// Extended Commands Implementation - Phase 3
//-----------------------------------------------------------------------------

static infra_error_t handle_incr_decr(memkv_conn_t* conn, bool increment) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 获取现有项
    memkv_item_t* old_item = NULL;
    if (poly_hashtable_get(g_context.store, conn->current_cmd.key, 
        (void**)&old_item) != INFRA_OK) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "NOT_FOUND\\r\\n");
    }
    
    // 解析当前值
    char* end;
    uint64_t current = strtoull(old_item->value, &end, 10);
    if (*end != '\0') {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "CLIENT_ERROR cannot increment or decrement non-numeric value\\r\\n");
    }
    
    // 计算新值
    uint64_t delta = strtoull(conn->current_cmd.key, NULL, 10);
    uint64_t new_value;
    if (increment) {
        new_value = current + delta;
    } else {
        new_value = (current > delta) ? (current - delta) : 0;
    }
    
    // 转换为字符串
    char value_str[32];
    int len = snprintf(value_str, sizeof(value_str), "%lu", new_value);
    if (len < 0 || len >= sizeof(value_str)) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "SERVER_ERROR value too large\\r\\n");
    }
    
    // 创建新项
    memkv_item_t* item = create_item(
        conn->current_cmd.key,
        value_str,
        len,
        old_item->flags,
        old_item->exptime ? 
            old_item->exptime - time(NULL) : 0
    );
    
    if (!item) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "SERVER_ERROR out of memory\\r\\n");
    }
    
    // 更新统计信息
    update_stats_delete(old_item->value_size);
    
    // 存储新项
    infra_error_t err = poly_hashtable_put(g_context.store, item->key, item);
    infra_mutex_unlock(&g_context.store_mutex);
    
    if (err == INFRA_OK) {
        update_stats_set(item->value_size);
        destroy_item(old_item);
        return memkv_send_response(conn, "%lu\\r\\n", new_value);
    } else {
        destroy_item(item);
        return memkv_send_response(conn, "SERVER_ERROR\\r\\n");
    }
}

static infra_error_t handle_incr(memkv_conn_t* conn) {
    return handle_incr_decr(conn, true);
}

static infra_error_t handle_decr(memkv_conn_t* conn) {
    return handle_incr_decr(conn, false);
}

static infra_error_t handle_touch(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 获取现有项
    memkv_item_t* old_item = NULL;
    if (poly_hashtable_get(g_context.store, conn->current_cmd.key, 
        (void**)&old_item) != INFRA_OK) {
        infra_mutex_unlock(&g_context.store_mutex);
        return memkv_send_response(conn, "NOT_FOUND\\r\\n");
    }
    
    // 更新过期时间
    old_item->exptime = conn->current_cmd.exptime ? 
        time(NULL) + conn->current_cmd.exptime : 0;
    
    infra_mutex_unlock(&g_context.store_mutex);
    return memkv_send_response(conn, "TOUCHED\\r\\n");
}

static infra_error_t handle_gat(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 获取现有项
    memkv_item_t* item = NULL;
    if (poly_hashtable_get(g_context.store, conn->current_cmd.key, 
        (void**)&item) != INFRA_OK || is_item_expired(item)) {
        infra_mutex_unlock(&g_context.store_mutex);
        update_stats_get(false);
        return memkv_send_response(conn, "END\\r\\n");
    }
    
    // 更新过期时间
    item->exptime = conn->current_cmd.exptime ? 
        time(NULL) + conn->current_cmd.exptime : 0;
    
    // 发送响应
    infra_error_t err = memkv_send_response(conn, 
        "VALUE %s %u %zu %lu\\r\\n", 
        item->key, item->flags, item->value_size, item->cas);
    
    if (err == INFRA_OK) {
        size_t bytes_sent = 0;
        err = infra_net_send(conn->socket, item->value, 
            item->value_size, &bytes_sent);
        
        if (err == INFRA_OK) {
            err = memkv_send_response(conn, "\\r\\n");
        }
    }
    
    infra_mutex_unlock(&g_context.store_mutex);
    update_stats_get(true);
    
    if (err == INFRA_OK) {
        err = memkv_send_response(conn, "END\\r\\n");
    }
    
    return err;
}

static infra_error_t handle_flush_all(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 清除所有项
    poly_hashtable_foreach(g_context.store, 
        (poly_hashtable_iter_fn)destroy_item, NULL);
    poly_hashtable_clear(g_context.store);
    
    // 重置统计信息
    memset(&g_context.stats, 0, sizeof(g_context.stats));
    
    infra_mutex_unlock(&g_context.store_mutex);
    return memkv_send_response(conn, "OK\\r\\n");
}
