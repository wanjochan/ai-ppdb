#include "internal/peer/peer_service.h"
#include "internal/poly/poly_memkv.h"
#include "internal/poly/poly_mux.h"
#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION          "1.0.0"
#define MEMKV_BUFFER_SIZE      8192
#define MEMKV_MAX_KEY_SIZE     250
#define MEMKV_MAX_VALUE_SIZE   (1024 * 1024)  // 1MB

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// memkv服务实现
typedef struct peer_memkv {
    peer_service_t base;        // 基类
    poly_memkv_t* store;       // 存储实例
    poly_mux_t* mux;           // 多路复用器
    uint16_t port;             // 监听端口
    bool is_running;           // 运行状态
} peer_memkv_t;

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

// Service interface functions
static infra_error_t init(const infra_config_t* config);
static infra_error_t cleanup(void);
static infra_error_t start(void);
static infra_error_t stop(void);
static bool is_running(void);
static infra_error_t cmd_handler(int argc, char** argv);

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

static const poly_cmd_option_t g_memkv_options[] = {
    {
        .name = "port",
        .desc = "Memcached server port",
        .has_value = true
    }
};

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

// 服务上下文
static struct {
    bool running;                // 服务是否运行
    poly_memkv_t* store;        // 存储实例
    poly_mux_t* mux;            // 多路复用器
    uint16_t port;              // 监听端口
    infra_mutex_t mutex;        // 全局互斥锁
} g_context = {0};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 读取值数据
static infra_error_t read_value(infra_socket_t sock, void** value, size_t bytes) {
    if (!value || bytes == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *value = malloc(bytes);
    if (!*value) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 读取值数据
    size_t read_bytes = 0;
    infra_error_t err = infra_net_recv(sock, *value, bytes, &read_bytes);
    if (err != INFRA_OK || read_bytes != bytes) {
        free(*value);
        *value = NULL;
        return err != INFRA_OK ? err : INFRA_ERROR_INVALID_STATE;
    }

    // 读取结束标记 "\r\n"
    char end_mark[2];
    size_t end_bytes = 0;
    err = infra_net_recv(sock, end_mark, 2, &end_bytes);
    if (err != INFRA_OK || end_bytes != 2 || end_mark[0] != '\r' || end_mark[1] != '\n') {
        free(*value);
        *value = NULL;
        INFRA_LOG_ERROR("Invalid value terminator: expected \\r\\n");
        return INFRA_ERROR_PROTOCOL;
    }
    
    return INFRA_OK;
}

// 读取命令
static infra_error_t read_command(infra_socket_t sock, char* cmd, size_t cmd_size,
    const char** args, size_t max_args, size_t* arg_count) {
    
    if (!cmd || !args || !arg_count) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 读取一行
    char line[1024];
    size_t pos = 0;
    size_t read_bytes = 0;

    while (pos < sizeof(line) - 1) {
        infra_error_t err = infra_net_recv(sock, &line[pos], 1, &read_bytes);
        if (err != INFRA_OK || read_bytes != 1) {
            return err != INFRA_OK ? err : INFRA_ERROR_INVALID_STATE;
        }

        if (line[pos] == '\n') {
            if (pos > 0 && line[pos - 1] == '\r') {
                line[pos - 1] = '\0';
                break;
            }
        }
        pos++;
    }

    if (pos >= sizeof(line) - 1) {
        return INFRA_ERROR_INVALID_STATE;
    }

    // 解析命令和参数
    char* token = strtok(line, " ");
    if (!token) {
        return INFRA_ERROR_INVALID_STATE;
    }

    // 复制命令
    strncpy(cmd, token, cmd_size - 1);
    cmd[cmd_size - 1] = '\0';

    // 解析参数
    *arg_count = 0;
    while ((token = strtok(NULL, " ")) != NULL && *arg_count < max_args) {
        args[*arg_count] = token;
        (*arg_count)++;
    }

    return INFRA_OK;
}

// 发送响应
static infra_error_t send_response(infra_socket_t sock, const char* response) {
    if (!response) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t len = strlen(response);
    size_t sent_bytes = 0;
    return infra_net_send(sock, response, len, &sent_bytes);
}

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

// 处理set命令
static infra_error_t handle_set(infra_socket_t sock, peer_memkv_t* memkv, 
    const char* key, uint32_t flags, uint32_t exptime, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(sock, &value, bytes);
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
        return send_response(sock, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理add命令
static infra_error_t handle_add(infra_socket_t sock, peer_memkv_t* memkv,
    const char* key, uint32_t flags, uint32_t exptime, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(sock, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 添加键值对
    err = poly_memkv_add(memkv->store, key, value, bytes, flags, exptime);
    free(value);

    if (err == INFRA_ERROR_ALREADY_EXISTS) {
        if (!noreply) {
            return send_response(sock, "NOT_STORED\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return send_response(sock, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理replace命令
static infra_error_t handle_replace(infra_socket_t sock, peer_memkv_t* memkv,
    const char* key, uint32_t flags, uint32_t exptime, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(sock, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 替换键值对
    err = poly_memkv_replace(memkv->store, key, value, bytes, flags, exptime);
    free(value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return send_response(sock, "NOT_STORED\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return send_response(sock, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理get/gets命令
static infra_error_t handle_get(infra_socket_t sock, peer_memkv_t* memkv,
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
            
            err = send_response(sock, header);
            if (err != INFRA_OK) {
                return err;
            }

            // 发送值数据
            err = infra_net_send(sock, item->value, item->value_size, NULL);
            if (err != INFRA_OK) {
                return err;
            }

            // 发送结束标记
            err = send_response(sock, "\r\n");
            if (err != INFRA_OK) {
                return err;
            }
        }
    }

    // 发送结束标记
    return send_response(sock, "END\r\n");
}

// 处理delete命令
static infra_error_t handle_delete(infra_socket_t sock, peer_memkv_t* memkv,
    const char* key, bool noreply) {
    
    infra_error_t err = poly_memkv_delete(memkv->store, key);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return send_response(sock, "NOT_FOUND\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return send_response(sock, "DELETED\r\n");
    }

    return INFRA_OK;
}

// 处理append命令
static infra_error_t handle_append(infra_socket_t sock, peer_memkv_t* memkv,
    const char* key, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(sock, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 追加数据
    err = poly_memkv_append(memkv->store, key, value, bytes);
    free(value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return send_response(sock, "NOT_STORED\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return send_response(sock, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理prepend命令
static infra_error_t handle_prepend(infra_socket_t sock, peer_memkv_t* memkv,
    const char* key, size_t bytes, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(sock, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // 前置数据
    err = poly_memkv_prepend(memkv->store, key, value, bytes);
    free(value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return send_response(sock, "NOT_STORED\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return send_response(sock, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理cas命令
static infra_error_t handle_cas(infra_socket_t sock, peer_memkv_t* memkv,
    const char* key, uint32_t flags, uint32_t exptime, size_t bytes, 
    uint64_t cas, bool noreply) {
    
    // 读取值数据
    void* value = NULL;
    infra_error_t err = read_value(sock, &value, bytes);
    if (err != INFRA_OK) {
        return err;
    }

    // CAS操作
    err = poly_memkv_cas(memkv->store, key, value, bytes, flags, exptime, cas);
    free(value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return send_response(sock, "NOT_FOUND\r\n");
        }
        return INFRA_OK;
    }

    if (err == INFRA_ERROR_CAS_MISMATCH) {
        if (!noreply) {
            return send_response(sock, "EXISTS\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return send_response(sock, "STORED\r\n");
    }

    return INFRA_OK;
}

// 处理flush_all命令
static infra_error_t handle_flush_all(infra_socket_t sock, peer_memkv_t* memkv,
    bool noreply) {
    
    infra_error_t err = poly_memkv_flush(memkv->store);
    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        return send_response(sock, "OK\r\n");
    }

    return INFRA_OK;
}

// 处理incr命令
static infra_error_t handle_incr(infra_socket_t sock, peer_memkv_t* memkv,
    const char* key, uint64_t delta, bool noreply) {
    
    uint64_t new_value = 0;
    infra_error_t err = poly_memkv_incr(memkv->store, key, delta, &new_value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return send_response(sock, "NOT_FOUND\r\n");
        }
        return INFRA_OK;
    }

    if (err == INFRA_ERROR_INVALID_TYPE) {
        if (!noreply) {
            return send_response(sock, "CLIENT_ERROR cannot increment non-numeric value\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        char response[32];
        snprintf(response, sizeof(response), "%lu\r\n", new_value);
        return send_response(sock, response);
    }

    return INFRA_OK;
}

// 处理decr命令
static infra_error_t handle_decr(infra_socket_t sock, peer_memkv_t* memkv,
    const char* key, uint64_t delta, bool noreply) {
    
    uint64_t new_value = 0;
    infra_error_t err = poly_memkv_decr(memkv->store, key, delta, &new_value);

    if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            return send_response(sock, "NOT_FOUND\r\n");
        }
        return INFRA_OK;
    }

    if (err == INFRA_ERROR_INVALID_TYPE) {
        if (!noreply) {
            return send_response(sock, "CLIENT_ERROR cannot decrement non-numeric value\r\n");
        }
        return INFRA_OK;
    }

    if (err != INFRA_OK) {
        return err;
    }

    // 发送响应
    if (!noreply) {
        char response[32];
        snprintf(response, sizeof(response), "%lu\r\n", new_value);
        return send_response(sock, response);
    }

    return INFRA_OK;
}

// 处理version命令
static infra_error_t handle_version(infra_socket_t sock) {
    char response[32];
    snprintf(response, sizeof(response), "VERSION %s\r\n", MEMKV_VERSION);
    return send_response(sock, response);
}

// 处理stats命令
static infra_error_t handle_stats(infra_socket_t sock, peer_memkv_t* memkv) {
    char buf[256];
    const poly_memkv_stats_t* stats = poly_memkv_get_stats(memkv->store);
    
    // 发送基本统计信息
    snprintf(buf, sizeof(buf), "STAT pid %d\r\n", getpid());
    send_response(sock, buf);
    
    snprintf(buf, sizeof(buf), "STAT version %s\r\n", MEMKV_VERSION);
    send_response(sock, buf);
    
    snprintf(buf, sizeof(buf), "STAT curr_items %zu\r\n", stats->curr_items);
    send_response(sock, buf);
    
    snprintf(buf, sizeof(buf), "STAT total_items %zu\r\n", stats->total_items);
    send_response(sock, buf);
    
    snprintf(buf, sizeof(buf), "STAT bytes %zu\r\n", stats->bytes);
    send_response(sock, buf);
    
    snprintf(buf, sizeof(buf), "STAT curr_connections %zu\r\n", stats->curr_connections);
    send_response(sock, buf);
    
    snprintf(buf, sizeof(buf), "STAT total_connections %zu\r\n", stats->total_connections);
    send_response(sock, buf);
    
    snprintf(buf, sizeof(buf), "STAT get_hits %zu\r\n", stats->hits);
    send_response(sock, buf);
    
    snprintf(buf, sizeof(buf), "STAT get_misses %zu\r\n", stats->misses);
    send_response(sock, buf);
    
    return send_response(sock, "END\r\n");
}

//-----------------------------------------------------------------------------
// Connection Handler
//-----------------------------------------------------------------------------

// 连接处理回调
static infra_error_t handle_connection(void* ctx, infra_socket_t sock) {
    peer_memkv_t* memkv = (peer_memkv_t*)ctx;
    if (!memkv) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 设置socket超时（30秒）
    infra_error_t err = infra_net_set_timeout(sock, 30000);  // 30秒 = 30000毫秒
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set socket timeout: %d", err);
        return err;
    }

    // 更新连接统计
    poly_atomic_inc(&memkv->store->stats.curr_connections);
    poly_atomic_inc(&memkv->store->stats.total_connections);

    // 读取命令
    char cmd[32];
    const char* args[16];
    size_t arg_count = 0;

    err = read_command(sock, cmd, sizeof(cmd), args, 16, &arg_count);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to read command: %d", err);
        poly_atomic_dec(&memkv->store->stats.curr_connections);
        return err;
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

        return handle_set(sock, memkv, key, flags, exptime, bytes, noreply);
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

        return handle_add(sock, memkv, key, flags, exptime, bytes, noreply);
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

        return handle_replace(sock, memkv, key, flags, exptime, bytes, noreply);
    }
    
    if (strcmp(cmd, "get") == 0 || strcmp(cmd, "gets") == 0) {
        if (arg_count < 1) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        return handle_get(sock, memkv, args, arg_count, cmd[3] == 's');
    }
    
    if (strcmp(cmd, "delete") == 0) {
        if (arg_count < 1) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        bool noreply = (arg_count > 1 && strcmp(args[1], "noreply") == 0);

        return handle_delete(sock, memkv, key, noreply);
    }
    
    if (strcmp(cmd, "append") == 0) {
        if (arg_count < 2) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        size_t bytes = atoi(args[1]);
        bool noreply = (arg_count > 2 && strcmp(args[2], "noreply") == 0);

        return handle_append(sock, memkv, key, bytes, noreply);
    }
    
    if (strcmp(cmd, "prepend") == 0) {
        if (arg_count < 2) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        size_t bytes = atoi(args[1]);
        bool noreply = (arg_count > 2 && strcmp(args[2], "noreply") == 0);

        return handle_prepend(sock, memkv, key, bytes, noreply);
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

        return handle_cas(sock, memkv, key, flags, exptime, bytes, cas, noreply);
    }
    
    if (strcmp(cmd, "flush_all") == 0) {
        bool noreply = (arg_count > 0 && strcmp(args[0], "noreply") == 0);
        return handle_flush_all(sock, memkv, noreply);
    }

    if (strcmp(cmd, "incr") == 0) {
        if (arg_count < 2) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        uint64_t delta = strtoull(args[1], NULL, 10);
        bool noreply = (arg_count > 2 && strcmp(args[2], "noreply") == 0);

        return handle_incr(sock, memkv, key, delta, noreply);
    }

    if (strcmp(cmd, "decr") == 0) {
        if (arg_count < 2) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        const char* key = args[0];
        uint64_t delta = strtoull(args[1], NULL, 10);
        bool noreply = (arg_count > 2 && strcmp(args[2], "noreply") == 0);

        return handle_decr(sock, memkv, key, delta, noreply);
    }

    if (strcmp(cmd, "version") == 0) {
        return handle_version(sock);
    }

    if (strcmp(cmd, "stats") == 0) {
        return handle_stats(sock, memkv);
    }

    return INFRA_ERROR_NOT_SUPPORTED;
}

//-----------------------------------------------------------------------------
// Service Implementation
//-----------------------------------------------------------------------------

static infra_error_t init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化存储实例
    poly_memkv_config_t store_config = {
        .initial_size = 1024,
        .max_key_size = MEMKV_MAX_KEY_SIZE,
        .max_value_size = MEMKV_MAX_VALUE_SIZE
    };

    infra_error_t err = poly_memkv_create(&store_config, &g_context.store);
    if (err != INFRA_OK) {
        return err;
    }

    // 初始化互斥锁
    err = infra_mutex_create(&g_context.mutex);
    if (err != INFRA_OK) {
        poly_memkv_destroy(g_context.store);
        g_context.store = NULL;
        return err;
    }

    g_context.port = 11211;  // 默认端口
    return INFRA_OK;
}

static infra_error_t cleanup(void) {
    if (g_context.mux) {
        poly_mux_stop(g_context.mux);
        poly_mux_destroy(g_context.mux);
        g_context.mux = NULL;
    }
    
    if (g_context.store) {
        poly_memkv_destroy(g_context.store);
        g_context.store = NULL;
    }

    infra_mutex_destroy(&g_context.mutex);
    return INFRA_OK;
}

// 获取实际绑定的端口号
static uint16_t get_bound_port(poly_mux_t* mux) {
    if (!mux) return 0;
    
    infra_socket_t listener = poly_mux_get_listener(mux);
    if (!listener) return 0;

    infra_net_addr_t addr = {0};
    infra_error_t err = infra_net_getsockname(listener, &addr);
    if (err != INFRA_OK) return 0;

    return addr.port;
}

static infra_error_t start(void) {
    if (g_context.running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // 创建多路复用器
    poly_mux_config_t config = {
        .port = g_context.port,
        .host = "0.0.0.0",
        .max_connections = 1000,
        .min_threads = 4,
        .max_threads = 16,
        .queue_size = 1000,
        .idle_timeout = 60
    };

    infra_error_t err = poly_mux_create(&config, &g_context.mux);
    if (err != INFRA_OK) {
        return err;
    }

    // 启动服务
    err = poly_mux_start(g_context.mux, handle_connection, &g_context);
    if (err != INFRA_OK) {
        poly_mux_destroy(g_context.mux);
        g_context.mux = NULL;
        g_context.running = false;
        return err;
    }

    g_context.running = true;
    g_context.port = get_bound_port(g_context.mux);
    INFRA_LOG_INFO("MemKV service started on port %d", g_context.port);
    return INFRA_OK;
}

static infra_error_t stop(void) {
    if (!g_context.running) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // 停止服务
    infra_error_t err = poly_mux_stop(g_context.mux);
    if (err != INFRA_OK) {
        return err;
    }

    poly_mux_destroy(g_context.mux);
    g_context.mux = NULL;
    g_context.running = false;
    INFRA_LOG_INFO("MemKV service stopped");
    return INFRA_OK;
}

static bool is_running(void) {
    return g_context.running;
}

static infra_error_t cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 解析命令行选项
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "--port") == 0) {
            if (i + 1 >= argc) {
                return INFRA_ERROR_INVALID_PARAM;
            }
            g_context.port = atoi(argv[++i]);
        } else if (strcmp(arg, "--start") == 0) {
            return start();
        } else if (strcmp(arg, "--stop") == 0) {
            return stop();
        } else if (strcmp(arg, "--status") == 0) {
            size_t curr_conns = 0, total_conns = 0;
            if (g_context.mux) {
                poly_mux_get_stats(g_context.mux, &curr_conns, &total_conns);
            }
            printf("MemKV service is %s\n", g_context.running ? "running" : "stopped");
            printf("Current connections: %zu\n", curr_conns);
            printf("Total connections: %zu\n", total_conns);
            return INFRA_OK;
        }
    }

    return INFRA_ERROR_INVALID_PARAM;
}

// 服务实例
peer_service_t g_memkv_service = {
    .config = {
        .name = "memkv",
        .type = SERVICE_TYPE_MEMKV,
        .options = g_memkv_options,
        .option_count = sizeof(g_memkv_options) / sizeof(g_memkv_options[0]),
        .config = NULL
    },
    .state = SERVICE_STATE_STOPPED,
    .init = init,
    .cleanup = cleanup,
    .start = start,
    .stop = stop,
    .is_running = is_running,
    .cmd_handler = cmd_handler
};
