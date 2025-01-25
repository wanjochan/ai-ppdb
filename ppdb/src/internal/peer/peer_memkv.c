// #include "internal/peer/peer_memkv.h"

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_platform.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/poly/poly_atomic.h"
#include "internal/peer/peer_service.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION          "1.0.0"
#define MEMKV_BUFFER_SIZE      8192
#define MEMKV_MAX_KEY_SIZE     250
#define MEMKV_MAX_VALUE_SIZE   (1024 * 1024)  // 1MB
#define MEMKV_MAX_CONNECTIONS  10000
#define MEMKV_DEFAULT_PORT     11211

// 线程池配置
#define MEMKV_MIN_THREADS      4
#define MEMKV_MAX_THREADS      32
#define MEMKV_QUEUE_SIZE       1000
#define MEMKV_IDLE_TIMEOUT     60

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// Item structure
typedef struct memkv_item {
    char* key;                  // Key
    void* value;                // Value
    size_t value_size;          // Value size
    uint32_t flags;             // Flags
    uint32_t exptime;           // Expiration time
    uint64_t cas;               // CAS value
    struct memkv_item* next;    // Next item in chain
} memkv_item_t;

// Statistics structure
typedef struct memkv_stats {
    poly_atomic_t cmd_get;      // Get commands
    poly_atomic_t cmd_set;      // Set commands
    poly_atomic_t cmd_delete;   // Delete commands
    poly_atomic_t hits;         // Cache hits
    poly_atomic_t misses;       // Cache misses
    poly_atomic_t curr_items;   // Current items
    poly_atomic_t total_items;  // Total items
    poly_atomic_t bytes;        // Current bytes used
} memkv_stats_t;

// Context structure
typedef struct memkv_context {
    bool is_running;                // Service running flag
    uint16_t port;                  // Listening port
    infra_socket_t sock;           // Listening socket
    infra_mutex_t mutex;           // Global mutex
    poly_hashtable_t* store;       // Key-value store
    poly_atomic_t cas_counter;     // CAS counter
    memkv_stats_t stats;           // Statistics
} memkv_context_t;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// // Global context
// extern memkv_context_t g_memkv_context;



#include "internal/peer/peer_service.h"
#include "internal/poly/poly_memkv.h"
#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// memkv服务实现
typedef struct peer_memkv {
    peer_service_t base;        // 基类
    poly_memkv_t* store;       // 存储实例
} peer_memkv_t;

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 读取值数据
static infra_error_t read_value(peer_connection_t* conn, void** value, size_t bytes) {
    if (!conn || !value || bytes == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *value = malloc(bytes);
    if (!*value) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 读取值数据
    size_t read_bytes = 0;
    infra_error_t err = peer_connection_read(conn, *value, bytes, &read_bytes);
    if (err != INFRA_OK || read_bytes != bytes) {
        free(*value);
        *value = NULL;
        return err != INFRA_OK ? err : INFRA_ERROR_INVALID_DATA;
    }

    // 读取结束标记 "\r\n"
    char end_mark[2];
    err = peer_connection_read(conn, end_mark, 2, &read_bytes);
    if (err != INFRA_OK || read_bytes != 2 || 
        end_mark[0] != '\r' || end_mark[1] != '\n') {
        free(*value);
        *value = NULL;
        return INFRA_ERROR_INVALID_DATA;
    }

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

// 处理set命令
static infra_error_t handle_set(peer_connection_t* conn, peer_memkv_t* memkv, 
    const char* key, uint32_t flags, uint32_t exptime, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(conn, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 设置键值对
    err = poly_memkv_set(memkv->store, key, value, bytes, flags, exptime);
    free(value);

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return peer_connection_write_string(conn, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理add命令
static infra_error_t handle_add(peer_connection_t* conn, peer_memkv_t* memkv,
    const char* key, uint32_t flags, uint32_t exptime, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(conn, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 添加键值对
    err = poly_memkv_add(memkv->store, key, value, bytes, flags, exptime);
    free(value);

    if (err == INFRA_ERROR_ALREADY_EXISTS) {
        if (!noreply) {
            return peer_connection_write_string(conn, "NOT_STORED\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return peer_connection_write_string(conn, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理replace命令
static infra_error_t handle_replace(peer_connection_t* conn, peer_memkv_t* memkv,
    const char* key, uint32_t flags, uint32_t exptime, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(conn, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 替换键值对
    err = poly_memkv_replace(memkv->store, key, value, bytes, flags, exptime);
    free(value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return peer_connection_write_string(conn, "NOT_STORED\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return peer_connection_write_string(conn, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理get/gets命令
static infra_error_t handle_get(peer_connection_t* conn, peer_memkv_t* memkv,
    const char** keys, size_t key_count, bool with_cas) {
    
    for (size_t i = 0; i < key_count; i++) {
        poly_memkv_item_t* item = NULL;
        infra_error_t err = poly_memkv_get(memkv->store, keys[i], &item);
        
        if (err == INFRA_OK && item) {
            // 发送响应头
            char header[128];
            if (with_cas) {
                snprintf(header, sizeof(header), "VALUE %s %u %zu %lu\r\n",
                    item->key, item->flags, item->value_size, item->cas);
            } else {
                snprintf(header, sizeof(header), "VALUE %s %u %zu\r\n",
                    item->key, item->flags, item->value_size);
            }
            
            err = peer_connection_write_string(conn, header);
            if (err != INFRA_OK) {
                return err;
            }

            // 发送值数据
            err = peer_connection_write(conn, item->value, item->value_size);
            if (err != INFRA_OK) {
                return err;
            }

            // 发送结束标记
            err = peer_connection_write_string(conn, "\r\n");
            if (err != INFRA_OK) {
                return err;
            }
        }
    }

    // 发送结束标记
    return peer_connection_write_string(conn, "END\r\n");
}

// 处理delete命令
static infra_error_t handle_delete(peer_connection_t* conn, peer_memkv_t* memkv,
    const char* key, bool noreply) {
    
    infra_error_t err = poly_memkv_delete(memkv->store, key);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return peer_connection_write_string(conn, "NOT_FOUND\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return peer_connection_write_string(conn, "DELETED\r\n");
    }

    return INFRA_OK;
}

// 处理append命令
static infra_error_t handle_append(peer_connection_t* conn, peer_memkv_t* memkv,
    const char* key, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(conn, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 追加数据
    err = poly_memkv_append(memkv->store, key, value, bytes);
    free(value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return peer_connection_write_string(conn, "NOT_STORED\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return peer_connection_write_string(conn, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理prepend命令
static infra_error_t handle_prepend(peer_connection_t* conn, peer_memkv_t* memkv,
    const char* key, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(conn, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 前置数据
    err = poly_memkv_prepend(memkv->store, key, value, bytes);
    free(value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return peer_connection_write_string(conn, "NOT_STORED\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return peer_connection_write_string(conn, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理cas命令
static infra_error_t handle_cas(peer_connection_t* conn, peer_memkv_t* memkv,
    const char* key, uint32_t flags, uint32_t exptime, size_t bytes, 
    uint64_t cas, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(conn, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // CAS操作
    err = poly_memkv_cas(memkv->store, key, value, bytes, flags, exptime, cas);
    free(value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return peer_connection_write_string(conn, "NOT_FOUND\r\n");
        }
        return INFRA_OK;
    }

    if (err == INFRA_ERROR_CAS_MISMATCH) {
        if (!noreply) {
            return peer_connection_write_string(conn, "EXISTS\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return peer_connection_write_string(conn, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理flush_all命令
static infra_error_t handle_flush_all(peer_connection_t* conn, peer_memkv_t* memkv,
    bool noreply) {
    
    infra_error_t err = poly_memkv_flush(memkv->store);
    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return peer_connection_write_string(conn, "OK\r\n");
    }

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Service Interface
//-----------------------------------------------------------------------------

// 处理命令
static infra_error_t handle_command(peer_connection_t* conn, peer_memkv_t* memkv,
    const char* cmd, const char** args, size_t arg_count) {
    
    if (!conn || !memkv || !cmd || !args) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 解析命令
    if (strcmp(cmd, "set") == 0) {
        if (arg_count < 4) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        uint32_t flags = atoi(args[1]);
        uint32_t exptime = atoi(args[2]);
        size_t bytes = atoi(args[3]);
        bool noreply = (arg_count > 4 && strcmp(args[4], "noreply") == 0);

        return handle_set(conn, memkv, key, flags, exptime, bytes, noreply);
    }
    
    if (strcmp(cmd, "add") == 0) {
        if (arg_count < 4) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        uint32_t flags = atoi(args[1]);
        uint32_t exptime = atoi(args[2]);
        size_t bytes = atoi(args[3]);
        bool noreply = (arg_count > 4 && strcmp(args[4], "noreply") == 0);

        return handle_add(conn, memkv, key, flags, exptime, bytes, noreply);
    }
    
    if (strcmp(cmd, "replace") == 0) {
        if (arg_count < 4) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        uint32_t flags = atoi(args[1]);
        uint32_t exptime = atoi(args[2]);
        size_t bytes = atoi(args[3]);
        bool noreply = (arg_count > 4 && strcmp(args[4], "noreply") == 0);

        return handle_replace(conn, memkv, key, flags, exptime, bytes, noreply);
    }
    
    if (strcmp(cmd, "get") == 0 || strcmp(cmd, "gets") == 0) {
        if (arg_count < 1) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        return handle_get(conn, memkv, args, arg_count, cmd[3] == 's');
    }
    
    if (strcmp(cmd, "delete") == 0) {
        if (arg_count < 1) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        bool noreply = (arg_count > 1 && strcmp(args[1], "noreply") == 0);

        return handle_delete(conn, memkv, key, noreply);
    }
    
    if (strcmp(cmd, "append") == 0) {
        if (arg_count < 2) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        size_t bytes = atoi(args[1]);
        bool noreply = (arg_count > 2 && strcmp(args[2], "noreply") == 0);

        return handle_append(conn, memkv, key, bytes, noreply);
    }
    
    if (strcmp(cmd, "prepend") == 0) {
        if (arg_count < 2) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        size_t bytes = atoi(args[1]);
        bool noreply = (arg_count > 2 && strcmp(args[2], "noreply") == 0);

        return handle_prepend(conn, memkv, key, bytes, noreply);
    }
    
    if (strcmp(cmd, "cas") == 0) {
        if (arg_count < 5) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        uint32_t flags = atoi(args[1]);
        uint32_t exptime = atoi(args[2]);
        size_t bytes = atoi(args[3]);
        uint64_t cas = strtoull(args[4], NULL, 10);
        bool noreply = (arg_count > 5 && strcmp(args[5], "noreply") == 0);

        return handle_cas(conn, memkv, key, flags, exptime, bytes, cas, noreply);
    }
    
    if (strcmp(cmd, "flush_all") == 0) {
        bool noreply = (arg_count > 0 && strcmp(args[0], "noreply") == 0);
        return handle_flush_all(conn, memkv, noreply);
    }

    return INFRA_ERROR_NOT_SUPPORTED;
}

// 处理连接
static infra_error_t on_connection(peer_service_t* service, peer_connection_t* conn) {
    if (!service || !conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    peer_memkv_t* memkv = (peer_memkv_t*)service;

    // 读取命令
    char cmd[32];
    const char* args[16];
    size_t arg_count = 0;

    infra_error_t err = peer_connection_read_command(conn, cmd, sizeof(cmd),
        args, 16, &arg_count);
    if (err != INFRA_OK) {
        return err;
    }

    // 处理命令
    return handle_command(conn, memkv, cmd, args, arg_count);
}

// 创建服务
static infra_error_t on_create(peer_service_t* service) {
    if (!service) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    peer_memkv_t* memkv = (peer_memkv_t*)service;

    // 创建存储实例
    poly_memkv_config_t config = {
        .initial_size = 1024,
        .max_key_size = 250,
        .max_value_size = 1024 * 1024
    };

    return poly_memkv_create(&config, &memkv->store);
}

// 销毁服务
static void on_destroy(peer_service_t* service) {
    if (!service) {
        return;
    }

    peer_memkv_t* memkv = (peer_memkv_t*)service;
    poly_memkv_destroy(memkv->store);
    free(service);
}

// 服务实例
peer_service_t g_memkv_service = {
    .name = "memkv",
    .on_connection = on_connection,
    .on_create = on_create,
    .on_destroy = on_destroy
};
