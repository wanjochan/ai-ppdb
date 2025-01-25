#include "internal/peer/peer_memkv_cmd.h"
#include "internal/peer/peer_memkv.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_platform.h"
#include "internal/poly/poly_hashtable.h"

// 前向声明
infra_error_t handle_set(memkv_conn_t* conn);
infra_error_t handle_add(memkv_conn_t* conn);
infra_error_t handle_replace(memkv_conn_t* conn);
infra_error_t handle_append(memkv_conn_t* conn);
infra_error_t handle_prepend(memkv_conn_t* conn);
infra_error_t handle_cas(memkv_conn_t* conn);
infra_error_t handle_get(memkv_conn_t* conn);
infra_error_t handle_gets(memkv_conn_t* conn);
infra_error_t handle_delete(memkv_conn_t* conn);
infra_error_t handle_incr(memkv_conn_t* conn);
infra_error_t handle_decr(memkv_conn_t* conn);
infra_error_t handle_touch(memkv_conn_t* conn);
infra_error_t handle_gat(memkv_conn_t* conn);
infra_error_t handle_flush_all(memkv_conn_t* conn);
infra_error_t handle_stats(memkv_conn_t* conn);
infra_error_t handle_version(memkv_conn_t* conn);
infra_error_t handle_quit(memkv_conn_t* conn);

// 命令处理器表
static const memkv_cmd_handler_t g_handlers[] = {
    {"set",     CMD_SET,     handle_set,     5, 5, true},
    {"add",     CMD_ADD,     handle_add,     5, 5, true},
    {"replace", CMD_REPLACE, handle_replace, 5, 5, true},
    {"append",  CMD_APPEND,  handle_append,  5, 5, true},
    {"prepend", CMD_PREPEND, handle_prepend, 5, 5, true},
    {"cas",     CMD_CAS,     handle_cas,     6, 6, true},
    {"get",     CMD_GET,     handle_get,     2, -1, false},
    {"gets",    CMD_GETS,    handle_gets,    2, -1, false},
    {"incr",     CMD_INCR,     handle_incr,     3, 3, false},
    {"decr",     CMD_DECR,     handle_decr,     3, 3, false},
    {"touch",    CMD_TOUCH,    handle_touch,    3, 3, false},
    {"gat",      CMD_GAT,      handle_gat,      3, -1, false},
    {"flush_all",CMD_FLUSH,    handle_flush_all,1, 2, false},
    {"delete",  CMD_DELETE,  handle_delete,  2, 2, false},
    {"stats",   CMD_STATS,   handle_stats,   1, 2, false},
    {"version", CMD_VERSION, handle_version, 1, 1, false},
    {"quit",    CMD_QUIT,    handle_quit,    1, 1, false},
    {NULL,      CMD_UNKNOWN, NULL,          0, 0, false}
};

// 全局上下文
extern memkv_context_t g_context;

// 存储操作
static infra_error_t store_with_lock(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    memkv_item_t* item = create_item(key, value, value_size, flags, exptime);
    if (!item) {
        return MEMKV_ERROR_NO_MEMORY;
    }

    // 获取锁
    infra_mutex_lock(&g_context.store_mutex);

    // 存储数据
    infra_error_t err = poly_hashtable_put(g_context.store, item->key, item);
    if (err == INFRA_OK) {
        update_stats_set(value_size);
    } else {
        destroy_item(item);
    }

    // 释放锁
    infra_mutex_unlock(&g_context.store_mutex);

    return err;
}

// 获取操作
static infra_error_t get_with_lock(const char* key, memkv_item_t** item) {
    infra_mutex_lock(&g_context.store_mutex);
    infra_error_t err = poly_hashtable_get(g_context.store, key, (void**)item);
    if (err == INFRA_OK && *item) {
        if (is_item_expired(*item)) {
            err = poly_hashtable_remove(g_context.store, key);
            if (err == INFRA_OK) {
                update_stats_delete((*item)->value_size);
                destroy_item(*item);
                *item = NULL;
            }
            err = MEMKV_ERROR_NOT_FOUND;
        }
    }
    infra_mutex_unlock(&g_context.store_mutex);
    return err;
}

// 删除操作
static infra_error_t delete_with_lock(const char* key) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(key, &item);
    if (err == INFRA_OK) {
        if (item) {
            err = poly_hashtable_remove(g_context.store, key);
            if (err == INFRA_OK) {
                update_stats_delete(item->value_size);
                destroy_item(item);
            }
        } else {
            err = MEMKV_ERROR_NOT_FOUND;
        }
    }
    return err;
}

// 发送值响应
static infra_error_t send_value_response(memkv_conn_t* conn, const memkv_item_t* item) {
    // 发送头部
    char header[256];
    size_t header_len = snprintf(header, sizeof(header), "VALUE %s %u %zu\r\n", 
        item->key, item->flags, item->value_size);
    infra_error_t err = send_response(conn, header, header_len);
    if (err != INFRA_OK) {
        return err;
    }

    // 发送值
    err = send_response(conn, item->value, item->value_size);
    if (err != INFRA_OK) {
        return err;
    }

    // 发送值结束标记
    return send_response(conn, "\r\n", 2);
}

// SET 命令处理
infra_error_t handle_set(memkv_conn_t* conn) {
    infra_error_t err = store_with_lock(conn->current_cmd.key, 
        conn->current_cmd.data, conn->current_cmd.bytes,
        conn->current_cmd.flags, conn->current_cmd.exptime);
    
    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    if (!conn->current_cmd.noreply) {
        return send_response(conn, "STORED\r\n", 8);
    }
    return INFRA_OK;
}

// ADD 命令处理
infra_error_t handle_add(memkv_conn_t* conn) {
    infra_error_t err = store_with_lock(conn->current_cmd.key, conn->current_cmd.data,
        conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
    
    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    if (!conn->current_cmd.noreply) {
        return send_response(conn, "STORED\r\n", 8);
    }
    return INFRA_OK;
}

// REPLACE 命令处理
infra_error_t handle_replace(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&old_item);
    if (err != INFRA_OK || !old_item) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    err = store_with_lock(conn->current_cmd.key, conn->current_cmd.data,
        conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
    
    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    if (!conn->current_cmd.noreply) {
        return send_response(conn, "STORED\r\n", 8);
    }
    return INFRA_OK;
}

// APPEND 命令处理
infra_error_t handle_append(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&old_item);
    if (err != INFRA_OK || !old_item) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    size_t new_size = ((memkv_item_t*)old_item)->value_size + conn->current_cmd.bytes;
    if (new_size > MEMKV_MAX_VALUE_SIZE) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "SERVER_ERROR value too large\r\n", 28);
        }
        return MEMKV_ERROR_BUFFER_FULL;
    }

    void* new_value = malloc(new_size);
    if (!new_value) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "SERVER_ERROR out of memory\r\n", 26);
        }
        return MEMKV_ERROR_NO_MEMORY;
    }

    memcpy(new_value, ((memkv_item_t*)old_item)->value, ((memkv_item_t*)old_item)->value_size);
    memcpy((char*)new_value + ((memkv_item_t*)old_item)->value_size, conn->current_cmd.data, conn->current_cmd.bytes);

    err = store_with_lock(conn->current_cmd.key, new_value, new_size,
        ((memkv_item_t*)old_item)->flags, ((memkv_item_t*)old_item)->exptime ? ((memkv_item_t*)old_item)->exptime - time(NULL) : 0);

    free(new_value);

    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    if (!conn->current_cmd.noreply) {
        return send_response(conn, "STORED\r\n", 8);
    }
    return INFRA_OK;
}

// PREPEND 命令处理
infra_error_t handle_prepend(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&old_item);
    if (err != INFRA_OK || !old_item) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    size_t new_size = ((memkv_item_t*)old_item)->value_size + conn->current_cmd.bytes;
    if (new_size > MEMKV_MAX_VALUE_SIZE) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "SERVER_ERROR value too large\r\n", 28);
        }
        return MEMKV_ERROR_BUFFER_FULL;
    }

    void* new_value = malloc(new_size);
    if (!new_value) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "SERVER_ERROR out of memory\r\n", 26);
        }
        return MEMKV_ERROR_NO_MEMORY;
    }

    memcpy(new_value, conn->current_cmd.data, conn->current_cmd.bytes);
    memcpy((char*)new_value + conn->current_cmd.bytes, ((memkv_item_t*)old_item)->value, ((memkv_item_t*)old_item)->value_size);

    err = store_with_lock(conn->current_cmd.key, new_value, new_size,
        ((memkv_item_t*)old_item)->flags, ((memkv_item_t*)old_item)->exptime ? ((memkv_item_t*)old_item)->exptime - time(NULL) : 0);

    free(new_value);

    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    if (!conn->current_cmd.noreply) {
        return send_response(conn, "STORED\r\n", 8);
    }
    return INFRA_OK;
}

// CAS 命令处理
infra_error_t handle_cas(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&old_item);
    if (err != INFRA_OK || !old_item) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_FOUND\r\n", 10);
        }
        return err;
    }

    if (((memkv_item_t*)old_item)->cas != conn->current_cmd.cas) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "EXISTS\r\n", 8);
        }
        return MEMKV_ERROR_EXISTS;
    }

    err = store_with_lock(conn->current_cmd.key, conn->current_cmd.data,
        conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
    
    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    if (!conn->current_cmd.noreply) {
        return send_response(conn, "STORED\r\n", 8);
    }
    return INFRA_OK;
}

// GET 命令处理
infra_error_t handle_get(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&item);
    if (err == INFRA_OK && item) {
        err = send_value_response(conn, item);
        if (err == INFRA_OK) {
            err = send_response(conn, "END\r\n", 5);
            if (err == INFRA_OK) {
                update_stats_get(true);
            }
        }
    } else {
        err = send_response(conn, "END\r\n", 5);
        if (err == INFRA_OK) {
            update_stats_get(false);
        }
    }
    return err;
}

// GETS 命令处理
infra_error_t handle_gets(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&item);
    if (err == INFRA_OK && item) {
        err = send_value_response(conn, item);
        if (err == INFRA_OK) {
            err = send_response(conn, "END\r\n", 5);
            if (err == INFRA_OK) {
                update_stats_get(true);
            }
        }
    } else {
        err = send_response(conn, "END\r\n", 5);
        if (err == INFRA_OK) {
            update_stats_get(false);
        }
    }
    return err;
}

// DELETE 命令处理
infra_error_t handle_delete(memkv_conn_t* conn) {
    infra_error_t err = delete_with_lock(conn->current_cmd.key);
    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_FOUND\r\n", 11);
        }
        return err;
    }

    if (!conn->current_cmd.noreply) {
        return send_response(conn, "DELETED\r\n", 9);
    }
    return INFRA_OK;
}

// INCR 命令处理
infra_error_t handle_incr(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&item);
    if (err != INFRA_OK || !item) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_FOUND\r\n", 10);
        }
        return err;
    }

    // 检查是否过期
    if (is_item_expired(item)) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_FOUND\r\n", 10);
        }
        return MEMKV_ERROR_NOT_FOUND;
    }

    // 解析当前值
    char* end;
    uint64_t current = strtoull(((memkv_item_t*)item)->value, &end, 10);
    if (*end != '\0') {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "CLIENT_ERROR cannot increment non-numeric value\r\n", 46);
        }
        return INFRA_ERROR_INVALID;
    }

    // 解析增量
    uint64_t delta = strtoull(conn->current_cmd.data, &end, 10);
    if (*end != '\0') {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "CLIENT_ERROR invalid increment value\r\n", 36);
        }
        return INFRA_ERROR_INVALID;
    }

    // 计算新值
    uint64_t new_value = current + delta;
    char value_str[32];
    int len = snprintf(value_str, sizeof(value_str), "%lu", new_value);
    if (len < 0 || len >= (int)sizeof(value_str)) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "SERVER_ERROR value too large\r\n", 28);
        }
        return INFRA_ERROR_INVALID;
    }

    // 存储新值
    err = store_with_lock(conn->current_cmd.key, value_str, len,
        ((memkv_item_t*)item)->flags, ((memkv_item_t*)item)->exptime);
    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    // 发送响应
    if (!conn->current_cmd.noreply) {
        char response[32];
        len = snprintf(response, sizeof(response), "%lu\r\n", new_value);
        return send_response(conn, response, len);
    }
    return INFRA_OK;
}

// DECR 命令处理
infra_error_t handle_decr(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&item);
    if (err != INFRA_OK || !item) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_FOUND\r\n", 10);
        }
        return err;
    }

    // 检查是否过期
    if (is_item_expired(item)) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_FOUND\r\n", 10);
        }
        return MEMKV_ERROR_NOT_FOUND;
    }

    // 解析当前值
    char* end;
    uint64_t current = strtoull(((memkv_item_t*)item)->value, &end, 10);
    if (*end != '\0') {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "CLIENT_ERROR cannot decrement non-numeric value\r\n", 46);
        }
        return INFRA_ERROR_INVALID;
    }

    // 解析减量
    uint64_t delta = strtoull(conn->current_cmd.data, &end, 10);
    if (*end != '\0') {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "CLIENT_ERROR invalid decrement value\r\n", 36);
        }
        return INFRA_ERROR_INVALID;
    }

    // 计算新值
    uint64_t new_value = (current > delta) ? (current - delta) : 0;
    char value_str[32];
    int len = snprintf(value_str, sizeof(value_str), "%lu", new_value);
    if (len < 0 || len >= (int)sizeof(value_str)) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "SERVER_ERROR value too large\r\n", 28);
        }
        return INFRA_ERROR_INVALID;
    }

    // 存储新值
    err = store_with_lock(conn->current_cmd.key, value_str, len,
        ((memkv_item_t*)item)->flags, ((memkv_item_t*)item)->exptime);
    if (err != INFRA_OK) {
        if (!conn->current_cmd.noreply) {
            return send_response(conn, "NOT_STORED\r\n", 11);
        }
        return err;
    }

    // 发送响应
    if (!conn->current_cmd.noreply) {
        char response[32];
        len = snprintf(response, sizeof(response), "%lu\r\n", new_value);
        return send_response(conn, response, len);
    }
    return INFRA_OK;
}

// TOUCH 命令处理
infra_error_t handle_touch(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&old_item);
    if (err == INFRA_OK && old_item) {
        ((memkv_item_t*)old_item)->exptime = conn->current_cmd.exptime ? time(NULL) + conn->current_cmd.exptime : 0;
    }
    return err;
}

// GAT 命令处理
infra_error_t handle_gat(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, (memkv_item_t**)&item);
    if (err == INFRA_OK && item) {
        ((memkv_item_t*)item)->exptime = conn->current_cmd.exptime ? time(NULL) + conn->current_cmd.exptime : 0;
        err = send_value_response(conn, (memkv_item_t*)item);
        update_stats_get(true);
    } else {
        update_stats_get(false);
    }
    return err;
}

// FLUSH_ALL 命令处理
infra_error_t handle_flush_all(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    if (poly_hashtable_is_iterating(g_context.store)) {
        infra_mutex_unlock(&g_context.store_mutex);
        return send_response(conn, "SERVER_ERROR hashtable is busy\r\n", 30);
    }
    poly_hashtable_clear(g_context.store);
    g_context.stats.curr_items = 0;
    g_context.stats.bytes = 0;
    infra_mutex_unlock(&g_context.store_mutex);
    return send_response(conn, "OK\r\n", 4);
}

// STATS 命令处理
infra_error_t handle_stats(memkv_conn_t* conn) {
    const memkv_stats_t* stats = &g_context.stats;
    char buffer[1024];
    int len = snprintf(buffer, sizeof(buffer),
        "STAT uptime %lu\r\n"
        "STAT cmd_get %d\r\n"
        "STAT cmd_set %d\r\n"
        "STAT cmd_delete %d\r\n"
        "STAT get_hits %d\r\n"
        "STAT get_misses %d\r\n"
        "STAT curr_items %d\r\n"
        "STAT total_items %d\r\n"
        "STAT bytes %d\r\n"
        "END\r\n",
        (unsigned long)(time(NULL) - g_context.start_time),
        (int)poly_atomic_get((poly_atomic_t*)&stats->cmd_get),
        (int)poly_atomic_get((poly_atomic_t*)&stats->cmd_set),
        (int)poly_atomic_get((poly_atomic_t*)&stats->cmd_delete),
        (int)poly_atomic_get((poly_atomic_t*)&stats->hits),
        (int)poly_atomic_get((poly_atomic_t*)&stats->misses),
        (int)poly_atomic_get((poly_atomic_t*)&stats->curr_items),
        (int)poly_atomic_get((poly_atomic_t*)&stats->total_items),
        (int)poly_atomic_get((poly_atomic_t*)&stats->bytes));

    return send_response(conn, buffer, len);
}

// VERSION 命令处理
infra_error_t handle_version(memkv_conn_t* conn) {
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "VERSION %s\r\n", MEMKV_VERSION);
    if (len < 0 || len >= sizeof(buffer)) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return send_response(conn, buffer, len);
}

// QUIT 命令处理
infra_error_t handle_quit(memkv_conn_t* conn) {
    return INFRA_ERROR_CLOSED;
}

//-----------------------------------------------------------------------------
// Command Processing
//-----------------------------------------------------------------------------

infra_error_t memkv_cmd_init(void) {
    // Initialize hash table
    infra_error_t err = poly_hashtable_create(1024, NULL, NULL, &g_context.store);
    if (err != INFRA_OK) {
        return err;
    }

    // Initialize mutex
    err = infra_mutex_create(&g_context.store_mutex);
    if (err != INFRA_OK) {
        poly_hashtable_destroy(g_context.store);
        g_context.store = NULL;
        return err;
    }

    return INFRA_OK;
}

infra_error_t memkv_cmd_cleanup(void) {
    if (g_context.store) {
        infra_mutex_lock(&g_context.store_mutex);
        poly_hashtable_clear(g_context.store);
        poly_hashtable_destroy(g_context.store);
        g_context.store = NULL;
        infra_mutex_unlock(&g_context.store_mutex);
    }

    if (g_context.store_mutex) {
        infra_mutex_destroy(&g_context.store_mutex);
        g_context.store_mutex = NULL;
    }

    return INFRA_OK;
}

infra_error_t memkv_cmd_process(memkv_conn_t* conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Parse command
    infra_error_t err = memkv_parse_command(conn);
    if (err != INFRA_OK) {
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            return err; // Need more data
        }
        send_response(conn, "ERROR\r\n", 7);
        return err;
    }

    // Find command handler
    const memkv_cmd_handler_t* handler = NULL;
    for (int i = 0; g_handlers[i].name != NULL; i++) {
        if (g_handlers[i].type == conn->current_cmd.type) {
            handler = &g_handlers[i];
            break;
        }
    }

    if (!handler) {
        send_response(conn, "ERROR\r\n", 7);
        return INFRA_ERROR_NOT_FOUND;
    }

    // Execute command
    err = handler->fn(conn);
    if (err != INFRA_OK) {
        if (err != INFRA_ERROR_WOULD_BLOCK) {
            send_response(conn, "ERROR\r\n", 7);
        }
        return err;
    }

    return INFRA_OK;
}

// 项目管理函数
memkv_item_t* create_item(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    if (!key || !value || value_size == 0) {
        return NULL;
    }

    size_t key_len = strlen(key);
    memkv_item_t* item = malloc(sizeof(memkv_item_t));
    if (!item) {
        return NULL;
    }

    item->key = strdup(key);
    if (!item->key) {
        free(item);
        return NULL;
    }

    item->value = malloc(value_size);
    if (!item->value) {
        free(item->key);
        free(item);
        return NULL;
    }

    memcpy(item->value, value, value_size);
    item->value_size = value_size;
    item->flags = flags;
    item->exptime = exptime ? time(NULL) + exptime : 0;
    item->cas = g_context.next_cas++;

    return item;
}

void destroy_item(memkv_item_t* item) {
    if (!item) {
        return;
    }
    if (item->key) {
        free(item->key);
    }
    if (item->value) {
        free(item->value);
    }
    free(item);
}

bool is_item_expired(const memkv_item_t* item) {
    if (!item || !item->exptime) {
        return false;
    }
    return time(NULL) > item->exptime;
}

// 统计函数
void update_stats_set(size_t bytes) {
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.cmd_set);
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.total_items);
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.curr_items);
    poly_atomic_add((poly_atomic_t*)&g_context.stats.bytes, bytes);
}

void update_stats_delete(size_t bytes) {
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.cmd_delete);
    poly_atomic_dec((poly_atomic_t*)&g_context.stats.curr_items);
    poly_atomic_sub((poly_atomic_t*)&g_context.stats.bytes, bytes);
}

void update_stats_get(bool hit) {
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.cmd_get);
    if (hit) {
        poly_atomic_inc((poly_atomic_t*)&g_context.stats.hits);
    } else {
        poly_atomic_inc((poly_atomic_t*)&g_context.stats.misses);
    }
}

// 通信函数
infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len) {
    if (!conn || !response) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t sent = 0;
    while (sent < len) {
        size_t bytes_sent = 0;
        infra_error_t err = infra_net_send(conn->sock, conn->response + sent, len - sent, &bytes_sent);
        if (err != INFRA_OK) {
            return err;
        }
        sent += bytes_sent;
    }
    return INFRA_OK;
}

infra_error_t memkv_parse_command(memkv_conn_t* conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Reset command state
    memset(&conn->current_cmd, 0, sizeof(memkv_cmd_t));
    conn->current_cmd.state = CMD_STATE_INIT;

    // Read command line
    char* line = conn->buffer;
    char* end = strchr(line, '\r');
    if (!end || end[1] != '\n') {
        return INFRA_ERROR_WOULD_BLOCK;
    }
    *end = '\0';

    // Parse command name
    char* token = strtok(line, " ");
    if (!token) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Find command handler
    const memkv_cmd_handler_t* handler = NULL;
    for (int i = 0; g_handlers[i].name != NULL; i++) {
        if (strcmp(token, g_handlers[i].name) == 0) {
            handler = &g_handlers[i];
            break;
        }
    }

    if (!handler) {
        return INFRA_ERROR_NOT_FOUND;
    }

    conn->current_cmd.type = handler->type;
    conn->current_cmd.state = CMD_STATE_COMPLETE;

    return INFRA_OK;
}
