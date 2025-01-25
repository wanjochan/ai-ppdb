#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_platform.h"
#include "internal/peer/peer_memkv.h"
#include "internal/peer/peer_memkv_cmd.h"
#include "internal/poly/poly_cmdline.h"

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

const poly_cmd_option_t memkv_options[] = {
    {
        .name = "port",
        .desc = "Port to listen on",
        .has_value = true,
    },
    {
        .name = "start",
        .desc = "Start the service",
        .has_value = false,
    },
    {
        .name = "stop",
        .desc = "Stop the service",
        .has_value = false,
    },
    {
        .name = "status",
        .desc = "Show service status",
        .has_value = false,
    },
};

const int memkv_option_count = sizeof(memkv_options) / sizeof(memkv_options[0]);

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

memkv_context_t g_context = {0};

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static infra_error_t create_listener(void);
static infra_error_t process_command(memkv_conn_t* conn);
static infra_error_t parse_command(memkv_conn_t* conn);
static infra_error_t execute_command(memkv_conn_t* conn);
static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn);
static void destroy_connection(memkv_conn_t* conn);
static void* handle_connection(void* arg);
static infra_error_t send_value_response(memkv_conn_t* conn, const memkv_item_t* item);

// Storage operations
static infra_error_t store_with_lock(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime);
static infra_error_t get_with_lock(const char* key, memkv_item_t** item);
static infra_error_t delete_with_lock(const char* key);

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// Create connection
static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn) {
    memkv_conn_t* new_conn = malloc(sizeof(memkv_conn_t));
    if (!new_conn) {
        return MEMKV_ERROR_NO_MEMORY;
    }

    memset(new_conn, 0, sizeof(memkv_conn_t));
    new_conn->sock = sock;
    new_conn->current_cmd.state = CMD_STATE_INIT;
    new_conn->is_active = true;
    new_conn->buffer_used = 0;
    new_conn->buffer_read = 0;

    // 分配缓冲区
    new_conn->buffer = malloc(MEMKV_BUFFER_SIZE);
    if (!new_conn->buffer) {
        free(new_conn);
        return MEMKV_ERROR_NO_MEMORY;
    }
    memset(new_conn->buffer, 0, MEMKV_BUFFER_SIZE);

    // 设置socket选项
    infra_error_t err;
    
    // 设置非阻塞模式
    err = infra_net_set_nonblock(sock, true);
    if (err != INFRA_OK) {
        destroy_connection(new_conn);
        return err;
    }

    // 设置读写超时
    err = infra_net_set_timeout(sock, 5000);  // 5秒超时
    if (err != INFRA_OK) {
        destroy_connection(new_conn);
        return err;
    }

    // 设置TCP_NODELAY
    err = infra_net_set_nodelay(sock, true);
    if (err != INFRA_OK) {
        destroy_connection(new_conn);
        return err;
    }

    // 设置TCP_KEEPALIVE
    err = infra_net_set_keepalive(sock, true);
    if (err != INFRA_OK) {
        destroy_connection(new_conn);
        return err;
    }

    INFRA_LOG_DEBUG("Connection created successfully");
    *conn = new_conn;
    return INFRA_OK;
}

// Destroy connection
static void destroy_connection(memkv_conn_t* conn) {
    if (!conn) return;
    
    // 关闭连接
    conn->is_active = false;
    
    // 清理命令相关资源
    if (conn->current_cmd.key) {
        free(conn->current_cmd.key);
        conn->current_cmd.key = NULL;
    }
    if (conn->current_cmd.data) {
        free(conn->current_cmd.data);
        conn->current_cmd.data = NULL;
    }
    
    // 清理缓冲区
    if (conn->buffer) {
        free(conn->buffer);
        conn->buffer = NULL;
    }
    
    // 关闭套接字
    if (conn->sock) {
        infra_net_close(conn->sock);
        conn->sock = NULL;
    }
    
    // 释放连接结构
    free(conn);
}

// Create item
memkv_item_t* create_item(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    memkv_item_t* item = (memkv_item_t*)malloc(sizeof(memkv_item_t));
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
    item->exptime = exptime ? (time(NULL) + exptime) : 0;
    item->cas = 0; // TODO: Generate CAS value
    item->ctime = time(NULL);
    item->atime = item->ctime;

    return item;
}

// Destroy item
void destroy_item(memkv_item_t* item) {
    if (item) {
        free(item->key);
        free(item->value);
        free(item);
    }
}

// Check if item is expired
bool is_item_expired(const memkv_item_t* item) {
    if (!item || !item->exptime) {
        return false;
    }
    return time(NULL) > item->exptime;
}

// Update statistics
void update_stats_set(size_t value_size) {
    poly_atomic_inc(&g_context.stats.cmd_set);
    poly_atomic_inc(&g_context.stats.curr_items);
    poly_atomic_inc(&g_context.stats.total_items);
    poly_atomic_add(&g_context.stats.bytes, value_size);
}

void update_stats_get(bool hit) {
    poly_atomic_inc(&g_context.stats.cmd_get);
    if (hit) {
        poly_atomic_inc(&g_context.stats.hits);
    } else {
        poly_atomic_inc(&g_context.stats.misses);
    }
}

void update_stats_delete(size_t value_size) {
    poly_atomic_inc(&g_context.stats.cmd_delete);
    poly_atomic_dec(&g_context.stats.curr_items);
    poly_atomic_sub(&g_context.stats.bytes, value_size);
}

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// Hash function
static uint64_t hash_fn(const void* key) {
    const char* str = (const char*)key;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Compare function
static bool compare_fn(const void* key1, const void* key2) {
    return strcmp((const char*)key1, (const char*)key2) == 0;
}

// Initialize MemKV
infra_error_t memkv_init(uint16_t port, const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Initialize global context
    memset(&g_context, 0, sizeof(g_context));
    g_context.port = port;

    // Create storage hash table
    infra_error_t err = poly_hashtable_create(1024, hash_fn, compare_fn, &g_context.store);
    if (err != INFRA_OK) {
        return err;
    }

    // Initialize mutex
    err = infra_mutex_create(&g_context.store_mutex);
    if (err != INFRA_OK) {
        poly_hashtable_destroy(g_context.store);
        return err;
    }

    // Create thread pool configuration
    infra_thread_pool_config_t pool_config = {
        .min_threads = MEMKV_MIN_THREADS,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = MEMKV_QUEUE_SIZE,
        .idle_timeout = MEMKV_IDLE_TIMEOUT
    };

    INFRA_LOG_DEBUG("Creating thread pool with config: min=%d, max=%d, queue=%d", 
        pool_config.min_threads, pool_config.max_threads, pool_config.queue_size);

    // Create thread pool
    err = infra_thread_pool_create(&pool_config, &g_context.pool);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create thread pool: %d", err);
        infra_mutex_destroy(g_context.store_mutex);
        poly_hashtable_destroy(g_context.store);
        return err;
    }

    INFRA_LOG_DEBUG("Thread pool created successfully");

    // Set start time
    g_context.start_time = time(NULL);

    return INFRA_OK;
}

infra_error_t memkv_cleanup(void) {
    if (g_context.is_running) {
        memkv_stop();
    }

    if (g_context.store) {
        // 获取锁
        infra_mutex_lock(&g_context.store_mutex);

        // 清理存储
        poly_hashtable_clear(g_context.store);

        // 释放锁
        infra_mutex_unlock(&g_context.store_mutex);
    }

    if (g_context.pool) {
        infra_thread_pool_destroy(g_context.pool);
        g_context.pool = NULL;
    }

    infra_net_close(g_context.listen_sock);
    g_context.listen_sock = NULL;
    infra_mutex_destroy(&g_context.store_mutex);

    return INFRA_OK;
}

static infra_error_t create_listener(void) {
    INFRA_LOG_DEBUG("Creating listener socket");
    
    // Create listen socket
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_socket_t listener = NULL;
    infra_error_t err = infra_net_create(&listener, false, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create socket: %d", err);
        return err;
    }

    if (listener == NULL) {
        INFRA_LOG_ERROR("Socket creation returned NULL");
        return INFRA_ERROR_RUNTIME;
    }

    INFRA_LOG_DEBUG("Socket created successfully");

    // Set address reuse
    err = infra_net_set_reuseaddr(listener, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set SO_REUSEADDR: %d", err);
        infra_net_close(listener);
        return err;
    }

    INFRA_LOG_DEBUG("SO_REUSEADDR set successfully");

    // Bind address
    infra_net_addr_t addr = {
        .host = "127.0.0.1",  // TODO g_context.host (oh no, need multiple config of [host,port]
        .port = g_context.port
    };
    
    INFRA_LOG_DEBUG("Binding to port %d", g_context.port);
    err = infra_net_bind(listener, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to bind socket: %d", err);
        infra_net_close(listener);
        return err;
    }

    INFRA_LOG_DEBUG("Socket bound successfully");

    // Start listening
    INFRA_LOG_DEBUG("Starting to listen");
    err = infra_net_listen(listener);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to listen: %d", err);
        infra_net_close(listener);
        return err;
    }

    INFRA_LOG_DEBUG("Listening started successfully");
    g_context.listen_sock = listener;
    return INFRA_OK;
}

infra_error_t memkv_start(void) {
    infra_error_t err;

    if (g_context.is_running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Create listen socket
    err = create_listener();
    if (err != INFRA_OK) {
        return err;
    }

    // 设置非阻塞模式
    err = infra_net_set_nonblock(g_context.listen_sock, true);
    if (err != INFRA_OK) {
        infra_net_close(g_context.listen_sock);
        g_context.listen_sock = NULL;
        return err;
    }

    // 标记服务已启动
    g_context.is_running = true;

    // 在前台运行
    INFRA_LOG_INFO("Starting memkv service in foreground on port %d", g_context.port);
    infra_printf("MemKV service started on port %d\n", g_context.port);
    
    while (g_context.is_running) {
        // 等待连接
        infra_socket_t client = NULL;
        infra_net_addr_t client_addr = {0};
        err = infra_net_accept(g_context.listen_sock, &client, &client_addr);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                continue;
            }
            INFRA_LOG_ERROR("Failed to accept connection: %d", err);
            break;
        }

        INFRA_LOG_INFO("Accepted connection from %s:%d", 
            client_addr.host, client_addr.port);

        // 创建连接结构
        memkv_conn_t* conn = NULL;
        err = create_connection(client, &conn);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to create connection: %d", err);
            infra_net_close(client);
            continue;
        }

        // 提交到线程池
        err = infra_thread_pool_submit(g_context.pool, handle_connection, conn);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to submit connection to thread pool: %d", err);
            destroy_connection(conn);
            continue;
        }

        INFRA_LOG_DEBUG("Connection submitted to thread pool successfully");
    }

    return INFRA_OK;
}

// 停止服务
infra_error_t memkv_stop(void) {
    if (!g_context.is_running) {
        return INFRA_ERROR_NOT_FOUND;
    }

    g_context.is_running = false;

    // 等待接受连接的线程退出
    if (g_context.accept_thread) {
        infra_thread_join(g_context.accept_thread);
        g_context.accept_thread = NULL;
    }

    if (g_context.listen_sock) {
        infra_net_close(g_context.listen_sock);
        g_context.listen_sock = NULL;
    }

    return INFRA_OK;
}

// 查询服务状态
bool memkv_is_running(void) {
    return g_context.is_running;
}

//-----------------------------------------------------------------------------
// Connection Handling
//-----------------------------------------------------------------------------

static infra_error_t parse_command(memkv_conn_t* conn) {
    if (!conn || !conn->buffer) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 查找命令结束符 \r\n
    char* cmd_end = strstr(conn->buffer, "\r\n");
    if (!cmd_end) {
        return INFRA_ERROR_WOULD_BLOCK; // 需要更多数据
    }

    // 计算命令长度
    size_t cmd_len = cmd_end - conn->buffer;
    if (cmd_len == 0) {
        // 空命令
        return INFRA_ERROR_INVALID;
    }

    // 创建命令字符串的副本（包括结尾的 \0）
    char* cmd_str = malloc(cmd_len + 1);
    if (!cmd_str) {
        return MEMKV_ERROR_NO_MEMORY;
    }
    memcpy(cmd_str, conn->buffer, cmd_len);
    cmd_str[cmd_len] = '\0';

    INFRA_LOG_DEBUG("Parsing command: %s", cmd_str);

    // 解析命令
    char cmd[16] = {0};
    char key[MEMKV_MAX_KEY_SIZE] = {0};
    uint32_t flags = 0;
    uint32_t exptime = 0;
    size_t bytes = 0;
    bool noreply = false;

    char* saveptr = NULL;
    char* token = strtok_r(cmd_str, " ", &saveptr);
    if (!token) {
        free(cmd_str);
        return INFRA_ERROR_INVALID;
    }
    strncpy(cmd, token, sizeof(cmd) - 1);

    // 初始化命令类型为未知
    conn->current_cmd.type = CMD_UNKNOWN;

    // 根据命令类型解析参数
    if (strcasecmp(cmd, "set") == 0) {
        conn->current_cmd.type = CMD_SET;
        // set <key> <flags> <exptime> <bytes> [noreply]\r\n
        token = strtok_r(NULL, " ", &saveptr); // key
        if (!token) goto parse_error;
        strncpy(key, token, sizeof(key) - 1);

        token = strtok_r(NULL, " ", &saveptr); // flags
        if (!token) goto parse_error;
        flags = atoi(token);

        token = strtok_r(NULL, " ", &saveptr); // exptime
        if (!token) goto parse_error;
        exptime = atoi(token);

        token = strtok_r(NULL, " ", &saveptr); // bytes
        if (!token) goto parse_error;
        bytes = atoi(token);

        token = strtok_r(NULL, " ", &saveptr); // noreply (optional)
        if (token && strcmp(token, "noreply") == 0) {
            noreply = true;
        }
    } else if (strcasecmp(cmd, "get") == 0) {
        conn->current_cmd.type = CMD_GET;
        // get <key>\r\n
        token = strtok_r(NULL, " ", &saveptr); // key
        if (!token) goto parse_error;
        strncpy(key, token, sizeof(key) - 1);
    } else if (strcasecmp(cmd, "delete") == 0) {
        conn->current_cmd.type = CMD_DELETE;
        // delete <key> [noreply]\r\n
        token = strtok_r(NULL, " ", &saveptr); // key
        if (!token) goto parse_error;
        strncpy(key, token, sizeof(key) - 1);

        token = strtok_r(NULL, " ", &saveptr); // noreply (optional)
        if (token && strcmp(token, "noreply") == 0) {
            noreply = true;
        }
    } else if (strcasecmp(cmd, "flush_all") == 0) {
        conn->current_cmd.type = CMD_FLUSH;
        // flush_all [exptime] [noreply]\r\n
        token = strtok_r(NULL, " ", &saveptr); // exptime (optional)
        if (token) {
            exptime = atoi(token);
            token = strtok_r(NULL, " ", &saveptr); // noreply (optional)
            if (token && strcmp(token, "noreply") == 0) {
                noreply = true;
            }
        }
    }

    // 检查命令类型是否有效
    if (conn->current_cmd.type == CMD_UNKNOWN) {
        INFRA_LOG_ERROR("Unknown command: %s", cmd);
        goto parse_error;
    }

    INFRA_LOG_DEBUG("Command parsed successfully: type=%d, key=%s", conn->current_cmd.type, key);

    // 保存命令参数
    if (key[0]) {
        conn->current_cmd.key = strdup(key);
        if (!conn->current_cmd.key) goto parse_error;
        conn->current_cmd.key_len = strlen(key);
    }
    conn->current_cmd.flags = flags;
    conn->current_cmd.exptime = exptime;
    conn->current_cmd.bytes = bytes;
    conn->current_cmd.noreply = noreply;

    // 移动缓冲区指针
    cmd_len = cmd_end - conn->buffer + 2;  // +2 for \r\n
    memmove(conn->buffer, conn->buffer + cmd_len, 
            conn->buffer_used - cmd_len);
    conn->buffer_used -= cmd_len;

    free(cmd_str);
    return INFRA_OK;

parse_error:
    free(cmd_str);
    return INFRA_ERROR_INVALID;
}

static infra_error_t execute_command(memkv_conn_t* conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_error_t err;
    memkv_item_t* item = NULL;

    INFRA_LOG_DEBUG("Executing command type: %d", conn->current_cmd.type);

    switch (conn->current_cmd.type) {
        case CMD_SET: {
            if (!conn->current_cmd.data || !conn->current_cmd.key) {
                return INFRA_ERROR_INVALID_PARAM;
            }

            err = store_with_lock(conn->current_cmd.key, 
                                conn->current_cmd.data,
                                conn->current_cmd.bytes,
                                conn->current_cmd.flags,
                                conn->current_cmd.exptime);
            
            if (err != INFRA_OK) {
                if (!conn->current_cmd.noreply) {
                    return send_response(conn, "NOT_STORED\r\n", 11);
                }
                return err;
            }

            if (!conn->current_cmd.noreply) {
                return send_response(conn, "STORED\r\n", 8);
            }
            break;
        }
        
        case CMD_GET: {
            if (!conn->current_cmd.key) {
                return INFRA_ERROR_INVALID_PARAM;
            }

            err = get_with_lock(conn->current_cmd.key, &item);
            if (err != INFRA_OK || !item) {
                err = send_response(conn, "END\r\n", 5);
                update_stats_get(false);
                return err;
            }

            if (is_item_expired(item)) {
                delete_with_lock(conn->current_cmd.key);
                err = send_response(conn, "END\r\n", 5);
                update_stats_get(false);
                return err;
            }

            err = send_value_response(conn, item);
            if (err == INFRA_OK) {
                err = send_response(conn, "END\r\n", 5);
            }
            update_stats_get(true);
            return err;
        }

        case CMD_DELETE: {
            if (!conn->current_cmd.key) {
                return INFRA_ERROR_INVALID_PARAM;
            }

            err = delete_with_lock(conn->current_cmd.key);
            if (err != INFRA_OK) {
                if (!conn->current_cmd.noreply) {
                    return send_response(conn, "NOT_FOUND\r\n", 11);
                }
                return err;
            }

            if (!conn->current_cmd.noreply) {
                return send_response(conn, "DELETED\r\n", 9);
            }
            break;
        }

        case CMD_FLUSH: {
            infra_mutex_lock(&g_context.store_mutex);
            poly_hashtable_foreach(g_context.store, (poly_hashtable_iter_fn)destroy_item, NULL);
            poly_hashtable_clear(g_context.store);
            infra_mutex_unlock(&g_context.store_mutex);
            
            if (!conn->current_cmd.noreply) {
                return send_response(conn, "OK\r\n", 4);
            }
            break;
        }

        default:
            INFRA_LOG_ERROR("Unknown command type: %d", conn->current_cmd.type);
            return send_response(conn, "ERROR\r\n", 7);
    }

    return INFRA_OK;
}

static infra_error_t process_command(memkv_conn_t* conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_error_t err;

    INFRA_LOG_DEBUG("Processing command in state: %d", conn->current_cmd.state);

    // 处理命令状态机
    switch (conn->current_cmd.state) {
        case CMD_STATE_INIT: {
            // 解析命令
            err = parse_command(conn);
            if (err != INFRA_OK) {
                if (err == INFRA_ERROR_WOULD_BLOCK) {
                    INFRA_LOG_DEBUG("Need more data for command");
                    return INFRA_OK; // 需要更多数据
                }
                INFRA_LOG_ERROR("Command parse error: %d", err);
                send_response(conn, "ERROR\r\n", 7);
                return err;
            }
            
            // 根据命令类型决定下一个状态
            if (conn->current_cmd.type == CMD_GET || 
                conn->current_cmd.type == CMD_DELETE ||
                conn->current_cmd.type == CMD_FLUSH) {
                conn->current_cmd.state = CMD_STATE_EXECUTING;
            } else if (conn->current_cmd.type == CMD_SET) {
                conn->current_cmd.state = CMD_STATE_READ_DATA;
            } else {
                INFRA_LOG_ERROR("Invalid command type: %d", conn->current_cmd.type);
                send_response(conn, "ERROR\r\n", 7);
                return INFRA_ERROR_INVALID;
            }
            break;
        }

        case CMD_STATE_READ_DATA: {
            // 检查是否有足够的数据
            if (conn->buffer_used < conn->current_cmd.bytes + 2) {
                INFRA_LOG_DEBUG("Need more data for value: %zu < %zu", 
                              conn->buffer_used, conn->current_cmd.bytes + 2);
                return INFRA_OK; // 需要更多数据
            }

            // 检查结束标记
            if (conn->buffer[conn->current_cmd.bytes] != '\r' ||
                conn->buffer[conn->current_cmd.bytes + 1] != '\n') {
                INFRA_LOG_ERROR("Bad data chunk terminator");
                send_response(conn, "CLIENT_ERROR bad data chunk\r\n", 28);
                return INFRA_ERROR_INVALID;
            }

            // 保存数据
            conn->current_cmd.data = malloc(conn->current_cmd.bytes);
            if (!conn->current_cmd.data) {
                INFRA_LOG_ERROR("Failed to allocate memory for data");
                send_response(conn, "SERVER_ERROR out of memory\r\n", 26);
                return MEMKV_ERROR_NO_MEMORY;
            }
            memcpy(conn->current_cmd.data, conn->buffer, conn->current_cmd.bytes);
            
            // 更新读取位置
            conn->buffer_read = conn->current_cmd.bytes + 2;
            conn->current_cmd.state = CMD_STATE_EXECUTING;
            break;
        }

        case CMD_STATE_EXECUTING: {
            // 执行命令
            err = execute_command(conn);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Command execution error: %d", err);
                return err;
            }
            conn->current_cmd.state = CMD_STATE_COMPLETE;
            break;
        }

        case CMD_STATE_COMPLETE:
            return INFRA_OK;

        default:
            INFRA_LOG_ERROR("Invalid command state: %d", conn->current_cmd.state);
            return INFRA_ERROR_INVALID;
    }

    return INFRA_OK;
}

infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len) {
    if (!conn || !response) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    INFRA_LOG_DEBUG("Sending response: %.*s", (int)len, response);

    size_t total_sent = 0;
    while (total_sent < len) {
        size_t sent = 0;
        infra_error_t err = infra_net_send(conn->sock, 
                                         response + total_sent, 
                                         len - total_sent, 
                                         &sent);
        
        if (err == INFRA_ERROR_TIMEOUT) {
            // 超时，继续尝试
            continue;
        } else if (err != INFRA_OK || sent == 0) {
            INFRA_LOG_ERROR("Failed to send response: %d", err);
            conn->is_active = false;
            return err;
        }

        total_sent += sent;
    }

    INFRA_LOG_DEBUG("Response sent successfully");
    return INFRA_OK;
}

static void* handle_connection(void* arg) {
    if (!arg) {
        return NULL;
    }

    memkv_conn_t* conn = (memkv_conn_t*)arg;
    infra_error_t err;

    while (conn->is_active) {
        // 读取命令
        size_t bytes_read = 0;
        err = infra_net_recv(conn->sock, 
                            conn->buffer + conn->buffer_used,
                            MEMKV_BUFFER_SIZE - conn->buffer_used,
                            &bytes_read);
        
        if (err == INFRA_ERROR_TIMEOUT) {
            // 超时，继续尝试
            continue;
        } else if (err != INFRA_OK || bytes_read == 0) {
            // 连接关闭或错误
            INFRA_LOG_DEBUG("Connection closed or error: %d", err);
            break;
        }

        INFRA_LOG_DEBUG("Received %zu bytes", bytes_read);
        conn->buffer_used += bytes_read;

        // 处理命令
        do {
            err = process_command(conn);
            if (err != INFRA_OK && err != INFRA_ERROR_WOULD_BLOCK) {
                INFRA_LOG_ERROR("Command processing error: %d", err);
                goto cleanup;
            }

            // 如果命令已完成，重置缓冲区
            if (conn->current_cmd.state == CMD_STATE_COMPLETE) {
                // 移动未处理的数据到缓冲区开始
                if (conn->buffer_used > conn->buffer_read) {
                    memmove(conn->buffer, 
                           conn->buffer + conn->buffer_read,
                           conn->buffer_used - conn->buffer_read);
                    conn->buffer_used -= conn->buffer_read;
                } else {
                    conn->buffer_used = 0;
                }
                conn->buffer_read = 0;
                
                // 重置命令状态
                memset(&conn->current_cmd, 0, sizeof(memkv_cmd_t));
                conn->current_cmd.state = CMD_STATE_INIT;
            }
        } while (err == INFRA_OK && conn->buffer_used > 0);
    }

cleanup:
    INFRA_LOG_DEBUG("Cleaning up connection");
    destroy_connection(conn);
    return NULL;
}

//-----------------------------------------------------------------------------
// Command Handler
//-----------------------------------------------------------------------------

infra_error_t memkv_cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        INFRA_LOG_ERROR("No command specified");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Parse options
    const char* port_str = NULL;
    bool start = false;
    bool stop = false;
    bool status = false;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            port_str = argv[i] + 7;
        } else if (strcmp(argv[i], "--start") == 0) {
            start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            stop = true;
        } else if (strcmp(argv[i], "--status") == 0) {
            status = true;
        }
    }

    // Process command
    if (status) {
        infra_printf("MemKV service is %s\n", 
            memkv_is_running() ? "running" : "stopped");
        return INFRA_OK;
    }

    if (stop) {
        return memkv_stop();
    }

    if (start) {
        uint16_t port = MEMKV_DEFAULT_PORT;
        if (port_str) {
            // Parse port
            char* endptr;
            long p = strtol(port_str, &endptr, 10);
            if (*endptr != '\0' || p <= 0 || p > 65535) {
                INFRA_LOG_ERROR("Invalid port: %s", port_str);
                return INFRA_ERROR_INVALID_PARAM;
            }
            port = (uint16_t)p;
        }

        INFRA_LOG_DEBUG("Initializing MemKV service on port %d", port);

        // Initialize service
        infra_config_t config = INFRA_DEFAULT_CONFIG;
        infra_error_t err = memkv_init(port, &config);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to initialize MemKV service: %d", err);
            return err;
        }

        INFRA_LOG_DEBUG("MemKV service initialized successfully");

        // Start service
        err = memkv_start();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start MemKV service: %d", err);
            memkv_cleanup();
            return err;
        }

        INFRA_LOG_INFO("MemKV service started on port %d", port);
        return INFRA_OK;
    }

    INFRA_LOG_ERROR("Invalid command");
    return INFRA_ERROR_INVALID_OPERATION;  // Invalid command
}

const memkv_stats_t* memkv_get_stats(void) {
    return &g_context.stats;
}

static infra_error_t send_value_response(memkv_conn_t* conn, const memkv_item_t* item) {
    char header[256];
    size_t header_len = snprintf(header, sizeof(header), "VALUE %s %u %zu\r\n", 
        item->key, item->flags, item->value_size);

    // 发送头部
    infra_error_t err = send_response(conn, header, header_len);
    if (err != INFRA_OK) {
        return err;
    }

    // 发送值
    size_t sent;
    err = infra_net_send(conn->sock, item->value, item->value_size, &sent);
    if (err != INFRA_OK || sent != item->value_size) {
        conn->is_active = false;
        return err;
    }

    // 发送结束标记
    return send_response(conn, "\r\n", 2);
}

// Storage operations
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
        update_stats_set(item->value_size);
    } else {
        destroy_item(item);
    }

    // 释放锁
    infra_mutex_unlock(&g_context.store_mutex);

    return err;
}

// 获取操作
static infra_error_t get_with_lock(const char* key, memkv_item_t** item) {
    // 获取锁
    infra_mutex_lock(&g_context.store_mutex);

    infra_error_t err = poly_hashtable_get(g_context.store, key, (void**)item);
    if (err == INFRA_OK && *item && is_item_expired(*item)) {
        err = poly_hashtable_remove(g_context.store, key);
        if (err == INFRA_OK) {
            update_stats_delete((*item)->value_size);
            destroy_item(*item);
            *item = NULL;
        }
        err = MEMKV_ERROR_NOT_FOUND;
    }

    // 释放锁
    infra_mutex_unlock(&g_context.store_mutex);

    return err;
}

// 删除操作
static infra_error_t delete_with_lock(const char* key) {
    // 获取锁
    infra_mutex_lock(&g_context.store_mutex);

    memkv_item_t* item = NULL;
    infra_error_t err = poly_hashtable_get(g_context.store, key, (void**)&item);
    if (err == INFRA_OK && item) {
        err = poly_hashtable_remove(g_context.store, key);
        if (err == INFRA_OK) {
            update_stats_delete(item->value_size);
            destroy_item(item);
        }
    } else {
        err = MEMKV_ERROR_NOT_FOUND;
    }

    // 释放锁
    infra_mutex_unlock(&g_context.store_mutex);

    return err;
}
