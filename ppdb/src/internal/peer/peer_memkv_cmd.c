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
    memkv_item_t* item = create_item(key, value, value_size, flags, exptime);
    if (!item) {
        return MEMKV_ERROR_NO_MEMORY;
    }

    // 存储数据
    infra_error_t err = poly_hashtable_put(g_context.store, item->key, item);
    if (err == INFRA_OK) {
        update_stats_set(item->value_size);
    } else {
        destroy_item(item);
    }

    return err;
}

// 获取操作
static infra_error_t get_with_lock(const char* key, memkv_item_t** item) {
    infra_error_t err = poly_hashtable_get(g_context.store, key, (void**)item);
    if (err == INFRA_OK && *item && is_item_expired(*item)) {
        err = poly_hashtable_delete(g_context.store, key);
        if (err == INFRA_OK) {
            update_stats_delete((*item)->value_size);
            destroy_item(*item);
            *item = NULL;
        }
        err = MEMKV_ERROR_NOT_FOUND;
    }
    return err;
}

// 删除操作
static infra_error_t delete_with_lock(const char* key) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(key, &item);
    if (err == INFRA_OK) {
        if (item) {
            err = poly_hashtable_delete(g_context.store, key);
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
    char header[256];
    size_t header_len = snprintf(header, sizeof(header), "VALUE %s %u %zu %lu\r\n", 
        item->key, item->flags, item->value_size, item->cas);

    infra_error_t err = send_response(conn, header, header_len);
    if (err == INFRA_OK) {
        size_t bytes_sent;
        err = infra_net_send(conn->sock, item->value, item->value_size, &bytes_sent);
        if (err == INFRA_OK) {
            err = send_response(conn, "\r\n", 2);
        }
    }
    return err;
}

// SET 命令处理
static infra_error_t handle_set(memkv_conn_t* conn) {
    return store_with_lock(conn->current_cmd.key, conn->current_cmd.data, 
        conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
}

// ADD 命令处理
static infra_error_t handle_add(memkv_conn_t* conn) {
    return store_with_lock(conn->current_cmd.key, conn->current_cmd.data,
        conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
}

// REPLACE 命令处理
static infra_error_t handle_replace(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        err = store_with_lock(conn->current_cmd.key, conn->current_cmd.data,
            conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
    }
    return err;
}

// APPEND 命令处理
static infra_error_t handle_append(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        size_t new_size = old_item->value_size + conn->current_cmd.bytes;
        if (new_size > MEMKV_MAX_VALUE_SIZE) {
            return MEMKV_ERROR_BUFFER_FULL;
        }

        void* new_value = malloc(new_size);
        if (!new_value) {
            return MEMKV_ERROR_NO_MEMORY;
        }

        memcpy(new_value, old_item->value, old_item->value_size);
        memcpy((char*)new_value + old_item->value_size, conn->current_cmd.data, conn->current_cmd.bytes);

        err = store_with_lock(conn->current_cmd.key, new_value, new_size,
            old_item->flags, old_item->exptime ? old_item->exptime - time(NULL) : 0);

        free(new_value);
    }
    return err;
}

// PREPEND 命令处理
static infra_error_t handle_prepend(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        size_t new_size = old_item->value_size + conn->current_cmd.bytes;
        if (new_size > MEMKV_MAX_VALUE_SIZE) {
            return MEMKV_ERROR_BUFFER_FULL;
        }

        void* new_value = malloc(new_size);
        if (!new_value) {
            return MEMKV_ERROR_NO_MEMORY;
        }

        memcpy(new_value, conn->current_cmd.data, conn->current_cmd.bytes);
        memcpy((char*)new_value + conn->current_cmd.bytes, old_item->value, old_item->value_size);

        err = store_with_lock(conn->current_cmd.key, new_value, new_size,
            old_item->flags, old_item->exptime ? old_item->exptime - time(NULL) : 0);

        free(new_value);
    }
    return err;
}

// CAS 命令处理
static infra_error_t handle_cas(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        if (old_item->cas != conn->current_cmd.cas) {
            return MEMKV_ERROR_EXISTS;
        }
        err = store_with_lock(conn->current_cmd.key, conn->current_cmd.data,
            conn->current_cmd.bytes, conn->current_cmd.flags, conn->current_cmd.exptime);
    }
    return err;
}

// GET 命令处理
static infra_error_t handle_get(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &item);
    if (err == INFRA_OK && item) {
        err = send_value_response(conn, item);
        update_stats_get(true);
    } else {
        update_stats_get(false);
    }
    return err;
}

// GETS 命令处理
static infra_error_t handle_gets(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &item);
    if (err == INFRA_OK && item) {
        err = send_value_response(conn, item);
        update_stats_get(true);
    } else {
        update_stats_get(false);
    }
    return err;
}

// DELETE 命令处理
static infra_error_t handle_delete(memkv_conn_t* conn) {
    return delete_with_lock(conn->current_cmd.key);
}

// INCR 命令处理
static infra_error_t handle_incr(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        char* end;
        uint64_t current = strtoull(old_item->value, &end, 10);
        if (*end != '\0') {
            return MEMKV_ERROR_CLIENT_ERROR;
        }

        uint64_t delta = strtoull(conn->current_cmd.key, NULL, 10);
        uint64_t new_value = current + delta;

        char value_str[32];
        size_t len = snprintf(value_str, sizeof(value_str), "%lu", new_value);
        if (len >= sizeof(value_str)) {
            return MEMKV_ERROR_BUFFER_FULL;
        }

        err = store_with_lock(conn->current_cmd.key, value_str, len,
            old_item->flags, old_item->exptime ? old_item->exptime - time(NULL) : 0);
    }
    return err;
}

// DECR 命令处理
static infra_error_t handle_decr(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        char* end;
        uint64_t current = strtoull(old_item->value, &end, 10);
        if (*end != '\0') {
            return MEMKV_ERROR_CLIENT_ERROR;
        }

        uint64_t delta = strtoull(conn->current_cmd.key, NULL, 10);
        uint64_t new_value = current > delta ? current - delta : 0;

        char value_str[32];
        size_t len = snprintf(value_str, sizeof(value_str), "%lu", new_value);
        if (len >= sizeof(value_str)) {
            return MEMKV_ERROR_BUFFER_FULL;
        }

        err = store_with_lock(conn->current_cmd.key, value_str, len,
            old_item->flags, old_item->exptime ? old_item->exptime - time(NULL) : 0);
    }
    return err;
}

// TOUCH 命令处理
static infra_error_t handle_touch(memkv_conn_t* conn) {
    memkv_item_t* old_item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &old_item);
    if (err == INFRA_OK && old_item) {
        old_item->exptime = conn->current_cmd.exptime ? time(NULL) + conn->current_cmd.exptime : 0;
    }
    return err;
}

// GAT 命令处理
static infra_error_t handle_gat(memkv_conn_t* conn) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(conn->current_cmd.key, &item);
    if (err == INFRA_OK && item) {
        item->exptime = conn->current_cmd.exptime ? time(NULL) + conn->current_cmd.exptime : 0;
        err = send_value_response(conn, item);
        update_stats_get(true);
    } else {
        update_stats_get(false);
    }
    return err;
}

// FLUSH_ALL 命令处理
static infra_error_t handle_flush_all(memkv_conn_t* conn) {
    poly_hashtable_foreach(g_context.store, (poly_hashtable_iter_fn)destroy_item, NULL);
    poly_hashtable_clear(g_context.store);
    return INFRA_OK;
}

// STATS 命令处理
static infra_error_t handle_stats(memkv_conn_t* conn) {
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
static infra_error_t handle_version(memkv_conn_t* conn) {
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "VERSION %s\r\n", MEMKV_VERSION);
    if (len < 0 || len >= sizeof(buffer)) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return send_response(conn, buffer, len);
}

// QUIT 命令处理
static infra_error_t handle_quit(memkv_conn_t* conn) {
    return INFRA_ERROR_CLOSED;
}
