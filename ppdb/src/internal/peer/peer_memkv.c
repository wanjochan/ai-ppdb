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

//-----------------------------------------------------------------------------
// 内部函数声明
//-----------------------------------------------------------------------------
static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn);
static void destroy_connection(memkv_conn_t* conn);
static void* handle_connection(void* arg);
static infra_error_t process_command(memkv_conn_t* conn);
static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn);
static void destroy_connection(memkv_conn_t* conn);
static infra_error_t send_value_response(memkv_conn_t* conn, const memkv_item_t* item);

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

    // 使用阻塞模式但设置超时
    infra_error_t err = infra_net_set_nonblock(sock, false);
    if (err != INFRA_OK) {
        free(new_conn);
        return err;
    }

    // 设置30秒超时
    err = infra_net_set_timeout(sock, 30000);
    if (err != INFRA_OK) {
        free(new_conn);
        return err;
    }

    // 分配缓冲区
    new_conn->buffer = malloc(MEMKV_BUFFER_SIZE);
    if (!new_conn->buffer) {
        free(new_conn);
        return MEMKV_ERROR_NO_MEMORY;
    }

    poly_atomic_inc(&g_context.stats.total_items);
    *conn = new_conn;
    return MEMKV_OK;
}

// Destroy connection
static void destroy_connection(memkv_conn_t* conn) {
    if (!conn) return;
    
    if (conn->current_cmd.key) {
        free(conn->current_cmd.key);
    }
    if (conn->current_cmd.data) {
        free(conn->current_cmd.data);
    }
    
    free(conn->buffer);
    infra_net_close(conn->sock);
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

    // 解析命令
    char cmd[16] = {0};
    char key[MEMKV_MAX_KEY_SIZE] = {0};
    uint32_t flags = 0;
    uint32_t exptime = 0;
    size_t bytes = 0;
    bool noreply = false;

    // 临时将\r\n替换为\0以便解析
    *cmd_end = '\0';
    char* cmd_str = conn->buffer;
    char* saveptr;
    char* token = strtok_r(cmd_str, " ", &saveptr);
    if (!token) {
        *cmd_end = '\r';
        return INFRA_ERROR_INVALID;
    }
    strncpy(cmd, token, sizeof(cmd) - 1);

    // 设置命令类型
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
    } else {
        goto parse_error;
    }

    // 保存命令参数
    conn->current_cmd.key = key[0] ? strdup(key) : NULL;
    conn->current_cmd.key_len = key[0] ? strlen(key) : 0;
    conn->current_cmd.flags = flags;
    conn->current_cmd.exptime = exptime;
    conn->current_cmd.bytes = bytes;
    conn->current_cmd.noreply = noreply;

    // 移动缓冲区指针
    size_t cmd_len = cmd_end - conn->buffer + 2;
    memmove(conn->buffer, conn->buffer + cmd_len, 
            conn->buffer_used - cmd_len);
    conn->buffer_used -= cmd_len;

    *cmd_end = '\r'; // 恢复\r\n
    return INFRA_OK;

parse_error:
    *cmd_end = '\r';
    return INFRA_ERROR_INVALID;
}

static infra_error_t execute_command(memkv_conn_t* conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    char response[MEMKV_BUFFER_SIZE];
    size_t response_len = 0;
    infra_error_t err;

    switch (conn->current_cmd.type) {
        case CMD_SET: {
            // 创建新项
            memkv_item_t* item = create_item(conn->current_cmd.key, 
                                           conn->current_cmd.data,
                                           conn->current_cmd.bytes,
                                           conn->current_cmd.flags,
                                           conn->current_cmd.exptime);
            if (!item) {
                response_len = snprintf(response, sizeof(response), 
                                     "SERVER_ERROR out of memory\r\n");
                return send_response(conn, response, response_len);
            }
            
            // 先删除旧项
            void* old_value = NULL;
            if (poly_hashtable_get(g_context.store, item->key, &old_value) == INFRA_OK) {
                memkv_item_t* old_item = (memkv_item_t*)old_value;
                poly_hashtable_remove(g_context.store, item->key);
                destroy_item(old_item);
            }
            
            // 存储到哈希表
            err = poly_hashtable_put(g_context.store, item->key, item);
            if (err != INFRA_OK) {
                response_len = snprintf(response, sizeof(response), 
                                     "SERVER_ERROR out of memory\r\n");
                destroy_item(item);
                return send_response(conn, response, response_len);
            }

            update_stats_set(conn->current_cmd.bytes);
            if (!conn->current_cmd.noreply) {
                response_len = snprintf(response, sizeof(response), 
                                     "STORED\r\n");
                return send_response(conn, response, response_len);
            }
            break;
        }
        case CMD_GET: {
            // 从哈希表获取
            void* value = NULL;
            err = poly_hashtable_get(g_context.store, conn->current_cmd.key, &value);
            if (err != INFRA_OK) {
                update_stats_get(false);
                response_len = snprintf(response, sizeof(response), "END\r\n");
                return send_response(conn, response, response_len);
            }

            memkv_item_t* item = (memkv_item_t*)value;
            if (is_item_expired(item)) {
                // 删除过期项
                poly_hashtable_remove(g_context.store, item->key);
                destroy_item(item);
                update_stats_get(false);
                response_len = snprintf(response, sizeof(response), "END\r\n");
                return send_response(conn, response, response_len);
            }

            // 发送值响应
            update_stats_get(true);
            err = send_value_response(conn, item);
            if (err == INFRA_OK) {
                err = send_response(conn, "END\r\n", 5);
            }
            return err;
        }
        case CMD_DELETE: {
            // 从哈希表删除
            void* value = NULL;
            err = poly_hashtable_get(g_context.store, conn->current_cmd.key, &value);
            if (err != INFRA_OK) {
                if (!conn->current_cmd.noreply) {
                    response_len = snprintf(response, sizeof(response), 
                                         "NOT_FOUND\r\n");
                    return send_response(conn, response, response_len);
                }
                break;
            }

            memkv_item_t* item = (memkv_item_t*)value;
            if (is_item_expired(item)) {
                if (!conn->current_cmd.noreply) {
                    response_len = snprintf(response, sizeof(response), 
                                         "NOT_FOUND\r\n");
                    return send_response(conn, response, response_len);
                }
                break;
            }

            poly_hashtable_remove(g_context.store, conn->current_cmd.key);
            update_stats_delete(item->value_size);
            destroy_item(item);

            if (!conn->current_cmd.noreply) {
                response_len = snprintf(response, sizeof(response), 
                                     "DELETED\r\n");
                return send_response(conn, response, response_len);
            }
            break;
        }
        case CMD_FLUSH: {
            // 清空所有数据
            poly_hashtable_clear(g_context.store);
            if (!conn->current_cmd.noreply) {
                response_len = snprintf(response, sizeof(response), 
                                     "OK\r\n");
                return send_response(conn, response, response_len);
            }
            break;
        }
        default:
            response_len = snprintf(response, sizeof(response), 
                                 "ERROR\r\n");
            return send_response(conn, response, response_len);
    }

    return INFRA_OK;
}

static infra_error_t process_command(memkv_conn_t* conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_error_t err;
    while (conn->is_active) {
        // 处理命令状态机
        switch (conn->current_cmd.state) {
            case CMD_STATE_INIT: {
                // 解析命令
                err = parse_command(conn);
                if (err != INFRA_OK) {
                    if (err == INFRA_ERROR_WOULD_BLOCK) {
                        return INFRA_OK; // 需要更多数据
                    }
                    send_response(conn, "ERROR\r\n", 7);
                    return err;
                }
                conn->current_cmd.state = CMD_STATE_READ_DATA;
                break;
            }

            case CMD_STATE_READ_DATA: {
                // 对于不需要数据的命令直接执行
                if (conn->current_cmd.type == CMD_GET || 
                    conn->current_cmd.type == CMD_DELETE ||
                    conn->current_cmd.type == CMD_FLUSH) {
                    conn->current_cmd.state = CMD_STATE_EXECUTING;
                    break;
                }

                // 检查是否有足够的数据
                if (conn->buffer_used < conn->current_cmd.bytes + 2) {
                    return INFRA_OK; // 需要更多数据
                }

                // 检查结束标记
                if (conn->buffer[conn->current_cmd.bytes] != '\r' ||
                    conn->buffer[conn->current_cmd.bytes + 1] != '\n') {
                    send_response(conn, "CLIENT_ERROR bad data chunk\r\n", 28);
                    return INFRA_ERROR_INVALID;
                }

                // 分配并复制数据
                conn->current_cmd.data = malloc(conn->current_cmd.bytes);
                if (!conn->current_cmd.data) {
                    send_response(conn, "SERVER_ERROR out of memory\r\n", 28);
                    return INFRA_ERROR_NO_MEMORY;
                }
                memcpy(conn->current_cmd.data, conn->buffer, 
                       conn->current_cmd.bytes);

                // 移动缓冲区
                memmove(conn->buffer,
                    conn->buffer + conn->current_cmd.bytes + 2,
                    conn->buffer_used - conn->current_cmd.bytes - 2);
                conn->buffer_used -= conn->current_cmd.bytes + 2;
                conn->current_cmd.state = CMD_STATE_EXECUTING;
                break;
            }

            case CMD_STATE_EXECUTING: {
                // 执行命令
                err = execute_command(conn);
                if (err != INFRA_OK) {
                    return err;
                }

                // 清理命令
                if (conn->current_cmd.key) {
                    free(conn->current_cmd.key);
                    conn->current_cmd.key = NULL;
                }
                if (conn->current_cmd.data) {
                    free(conn->current_cmd.data);
                    conn->current_cmd.data = NULL;
                }
                
                conn->current_cmd.state = CMD_STATE_INIT;
                break;
            }

            default:
                return INFRA_ERROR_INVALID;
        }
    }

    return INFRA_OK;
}

infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len) {
    if (!conn || !response) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t sent;
    infra_error_t err = infra_net_send(conn->sock, response, len, &sent);
    if (err != INFRA_OK || sent != len) {
        conn->is_active = false;
        return err;
    }
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
        size_t bytes_read;
        err = infra_net_recv(conn->sock, 
                            conn->buffer + conn->buffer_used,
                            MEMKV_BUFFER_SIZE - conn->buffer_used,
                            &bytes_read);
        if (err != INFRA_OK || bytes_read == 0) {
            break;  // 错误或连接关闭
        }

        conn->buffer_used += bytes_read;

        // 处理命令
        err = process_command(conn);
        if (err != INFRA_OK) {
            break;
        }
    }

    // 清理连接
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
