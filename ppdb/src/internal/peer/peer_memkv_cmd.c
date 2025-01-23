#include "internal/peer/peer_memkv_cmd.h"
#include "internal/peer/peer_memkv.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_platform.h"
#include "internal/poly/poly_hashtable.h"

// 前向声明
static infra_error_t handle_set(memkv_conn_t* conn);
static infra_error_t handle_add(memkv_conn_t* conn);
static infra_error_t handle_replace(memkv_conn_t* conn);
static infra_error_t handle_append(memkv_conn_t* conn);
static infra_error_t handle_prepend(memkv_conn_t* conn);
static infra_error_t handle_cas(memkv_conn_t* conn);
static infra_error_t handle_get(memkv_conn_t* conn);
static infra_error_t handle_gets(memkv_conn_t* conn);
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
    infra_mutex_lock(&g_context.store_mutex);
    
    memkv_item_t* item = create_item(key, value, value_size, flags, exptime);
    if (!item) {
        infra_mutex_unlock(&g_context.store_mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    infra_error_t err = poly_hashtable_put(g_context.store, key, item);
    if (err == INFRA_OK) {
        update_stats_set(item->value_size);
    } else {
        destroy_item(item);
    }

    infra_mutex_unlock(&g_context.store_mutex);
    return err;
}

// 获取操作
static infra_error_t get_with_lock(const char* key, memkv_item_t** item) {
    infra_mutex_lock(&g_context.store_mutex);
    
    infra_error_t err = poly_hashtable_get(g_context.store, key, (void**)item);
    
    // 检查项是否过期
    if (err == INFRA_OK && *item && is_item_expired(*item)) {
        err = INFRA_ERROR_NOT_FOUND;
    }

    infra_mutex_unlock(&g_context.store_mutex);
    return err;
}

// 删除操作
static infra_error_t delete_with_lock(const char* key) {
    infra_mutex_lock(&g_context.store_mutex);
    
    memkv_item_t* item = NULL;
    infra_error_t err = poly_hashtable_get(g_context.store, key, (void**)&item);
    
    if (err == INFRA_OK) {
        if (!is_item_expired(item)) {
            err = poly_hashtable_delete(g_context.store, key);
            if (err == INFRA_OK) {
                update_stats_delete(item->value_size);
                destroy_item(item);
            }
        } else {
            err = INFRA_ERROR_NOT_FOUND;
        }
    }

    infra_mutex_unlock(&g_context.store_mutex);
    return err;
}

// 发送值响应
static infra_error_t send_value_response(memkv_conn_t* conn, memkv_item_t* item) {
    infra_error_t err = memkv_send_response(conn, "VALUE %s %u %zu %lu\r\n", item->key, item->flags, item->value_size, item->cas);
    if (err == INFRA_OK) {
        size_t bytes_sent = 0;
        err = infra_net_send(conn->socket, item->value, item->value_size, &bytes_sent);
        if (err == INFRA_OK) {
            err = memkv_send_response(conn, "\r\n");
        }
    }
    return err;
}

// 检查数值类型
static bool is_numeric_value(const char* value) {
    if (!value) return false;
    
    char* end;
    strtoull(value, &end, 10);
    return *end == '\0';
}

// 命令处理器
static infra_error_t handle_set(memkv_conn_t* conn) {
    return store_with_lock(conn->current_cmd.key, conn->current_cmd.data, conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
}

static infra_error_t handle_add(memkv_conn_t* conn) {
    return store_with_lock(conn->current_cmd.key, conn->current_cmd.data, conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
}

static infra_error_t handle_replace(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        err = store_with_lock(conn->current_cmd.key, conn->current_cmd.data, conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
        if (err == INFRA_OK) {
            destroy_item(old_item);
        }
    } else {
        err = INFRA_ERROR_NOT_FOUND;
    }
    return err;
}

static infra_error_t handle_append(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        size_t new_size = old_item->value_size + conn->current_cmd.bytes;
        void* new_value = malloc(new_size);
        if (!new_value) {
            return INFRA_ERROR_NO_MEMORY;
        }
        
        memcpy(new_value, old_item->value, old_item->value_size);
        memcpy((char*)new_value + old_item->value_size, conn->current_cmd.data, conn->current_cmd.bytes);
        
        err = store_with_lock(conn->current_cmd.key, new_value, new_size, old_item->flags, old_item->exptime ? old_item->exptime - time(NULL) : 0);
        free(new_value);
        if (err == INFRA_OK) {
            destroy_item(old_item);
        }
    } else {
        err = INFRA_ERROR_NOT_FOUND;
    }
    return err;
}

static infra_error_t handle_prepend(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        size_t new_size = old_item->value_size + conn->current_cmd.bytes;
        void* new_value = malloc(new_size);
        if (!new_value) {
            return INFRA_ERROR_NO_MEMORY;
        }
        
        memcpy(new_value, conn->current_cmd.data, conn->current_cmd.bytes);
        memcpy((char*)new_value + conn->current_cmd.bytes, old_item->value, old_item->value_size);
        
        err = store_with_lock(conn->current_cmd.key, new_value, new_size, old_item->flags, old_item->exptime ? old_item->exptime - time(NULL) : 0);
        free(new_value);
        if (err == INFRA_OK) {
            destroy_item(old_item);
        }
    } else {
        err = INFRA_ERROR_NOT_FOUND;
    }
    return err;
}

static infra_error_t handle_cas(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        if (old_item->cas != conn->current_cmd.cas) {
            return INFRA_ERROR_EXISTS;
        }
        err = store_with_lock(conn->current_cmd.key, conn->current_cmd.data, conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
        if (err == INFRA_OK) {
            destroy_item(old_item);
        }
    } else {
        err = INFRA_ERROR_NOT_FOUND;
    }
    return err;
}

static infra_error_t handle_get(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &item);
    if (err == INFRA_OK && item) {
        err = send_value_response(conn, item);
        if (err == INFRA_OK) {
            err = memkv_send_response(conn, "END\r\n");
        }
    }
    return err;
}

static infra_error_t handle_gets(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &item);
    if (err == INFRA_OK && item) {
        err = send_value_response(conn, item);
        if (err == INFRA_OK) {
            err = memkv_send_response(conn, "END\r\n");
        }
    }
    return err;
}

static infra_error_t handle_delete(memkv_conn_t* conn) {
    return delete_with_lock(conn->current_cmd.key);
}

static infra_error_t handle_incr(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        char* end;
        uint64_t current = strtoull(old_item->value, &end, 10);
        if (*end != '\0') {
            return INFRA_ERROR_CLIENT_ERROR;
        }
        
        uint64_t delta = strtoull(conn->current_cmd.key, NULL, 10);
        uint64_t new_value = current + delta;
        
        char value_str[32];
        int len = snprintf(value_str, sizeof(value_str), "%lu", new_value);
        if (len < 0 || len >= sizeof(value_str)) {
            return INFRA_ERROR_BUFFER_FULL;
        }
        
        err = store_with_lock(conn->current_cmd.key, value_str, len, old_item->flags, old_item->exptime ? old_item->exptime - time(NULL) : 0);
        if (err == INFRA_OK) {
            destroy_item(old_item);
        }
    } else {
        err = INFRA_ERROR_NOT_FOUND;
    }
    return err;
}

static infra_error_t handle_decr(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        char* end;
        uint64_t current = strtoull(old_item->value, &end, 10);
        if (*end != '\0') {
            return INFRA_ERROR_CLIENT_ERROR;
        }
        
        uint64_t delta = strtoull(conn->current_cmd.key, NULL, 10);
        uint64_t new_value = (current > delta) ? (current - delta) : 0;
        
        char value_str[32];
        int len = snprintf(value_str, sizeof(value_str), "%lu", new_value);
        if (len < 0 || len >= sizeof(value_str)) {
            return INFRA_ERROR_BUFFER_FULL;
        }
        
        err = store_with_lock(conn->current_cmd.key, value_str, len, old_item->flags, old_item->exptime ? old_item->exptime - time(NULL) : 0);
        if (err == INFRA_OK) {
            destroy_item(old_item);
        }
    } else {
        err = INFRA_ERROR_NOT_FOUND;
    }
    return err;
}

static infra_error_t handle_touch(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        old_item->exptime = conn->current_cmd.exptime ? time(NULL) + conn->current_cmd.exptime : 0;
    } else {
        err = INFRA_ERROR_NOT_FOUND;
    }
    return err;
}

static infra_error_t handle_gat(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &item);
    if (err == INFRA_OK && item) {
        item->exptime = conn->current_cmd.exptime ? time(NULL) + conn->current_cmd.exptime : 0;
        err = send_value_response(conn, item);
        if (err == INFRA_OK) {
            err = memkv_send_response(conn, "END\r\n");
        }
    } else {
        err = INFRA_ERROR_NOT_FOUND;
    }
    return err;
}

static infra_error_t handle_flush_all(memkv_conn_t* conn) {
    infra_mutex_lock(&g_context.store_mutex);
    
    // 清除所有项
    poly_hashtable_foreach(g_context.store, (poly_hashtable_iter_fn)destroy_item, NULL);
    poly_hashtable_clear(g_context.store);
    
    // 重置统计信息
    memset(&g_context.stats, 0, sizeof(g_context.stats));
    
    infra_mutex_unlock(&g_context.store_mutex);
    return memkv_send_response(conn, "OK\r\n");
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
