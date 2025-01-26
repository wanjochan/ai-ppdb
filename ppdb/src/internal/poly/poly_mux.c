#include "internal/poly/poly_mux.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 复用器结构
struct poly_mux {
    bool running;                    // 运行状态
    infra_thread_pool_t* pool;       // 线程池
    infra_mutex_t mutex;             // 互斥锁
    poly_mux_events_t events;        // 事件处理器
    void* event_ctx;                 // 事件上下文
    poly_mux_conn_t* conns;         // 连接列表
    uint32_t curr_conns;            // 当前连接数
    uint32_t total_conns;           // 总连接数
    poly_mux_config_t config;        // 配置
};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 创建连接对象
static poly_mux_conn_t* create_conn(poly_mux_t* mux, infra_socket_t sock, poly_mux_conn_type_t type) {
    if (!mux || !sock) return NULL;

    // 分配内存
    poly_mux_conn_t* conn = malloc(sizeof(poly_mux_conn_t));
    if (!conn) {
        INFRA_LOG_ERROR("Failed to allocate connection");
        return NULL;
    }
    memset(conn, 0, sizeof(poly_mux_conn_t));

    // 初始化连接
    conn->type = type;
    conn->state = POLY_MUX_CONN_STATE_NONE;
    conn->sock = sock;
    conn->mux = mux;
    conn->last_active = infra_time_ms();

    // 分配缓冲区
    conn->read_buffer = malloc(mux->config.conn_config.read_buffer_size);
    if (!conn->read_buffer) {
        INFRA_LOG_ERROR("Failed to allocate read buffer");
        free(conn);
        return NULL;
    }

    conn->write_buffer = malloc(mux->config.conn_config.write_buffer_size);
    if (!conn->write_buffer) {
        INFRA_LOG_ERROR("Failed to allocate write buffer");
        free(conn->read_buffer);
        free(conn);
        return NULL;
    }

    // 设置非阻塞
    if (mux->config.conn_config.nonblocking) {
        infra_error_t err = infra_net_set_nonblock(sock, true);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to set nonblock: %d", err);
            free(conn->write_buffer);
            free(conn->read_buffer);
            free(conn);
            return NULL;
        }
    }

    return conn;
}

// 释放连接资源
static void free_conn(poly_mux_conn_t* conn) {
    if (!conn) return;

    if (conn->sock) {
        infra_net_close(conn->sock);
        conn->sock = NULL;
    }

    if (conn->read_buffer) {
        free(conn->read_buffer);
        conn->read_buffer = NULL;
    }

    if (conn->write_buffer) {
        free(conn->write_buffer);
        conn->write_buffer = NULL;
    }

    free(conn);
}

// 从连接列表中移除连接
static void remove_conn(poly_mux_t* mux, poly_mux_conn_t* conn) {
    if (!mux || !conn) return;

    infra_mutex_lock(&mux->mutex);

    // 从链表中移除
    poly_mux_conn_t** pp = &mux->conns;
    while (*pp) {
        if (*pp == conn) {
            *pp = conn->next;
            mux->curr_conns--;
            break;
        }
        pp = &(*pp)->next;
    }

    infra_mutex_unlock(&mux->mutex);

    // 通知关闭
    if (mux->events.on_close) {
        mux->events.on_close(mux->event_ctx, conn);
    }

    // 释放资源
    free_conn(conn);
}

// 处理可读事件
static void handle_readable(poly_mux_conn_t* conn) {
    if (!conn || !conn->sock) return;

    // 读取数据
    size_t space = conn->mux->config.conn_config.read_buffer_size - conn->read_pos;
    if (space == 0) {
        INFRA_LOG_ERROR("Read buffer full");
        conn->state = POLY_MUX_CONN_STATE_CLOSING;
        return;
    }

    size_t bytes_read = 0;
    infra_error_t err = infra_net_recv(conn->sock,
        conn->read_buffer + conn->read_pos,
        space,
        &bytes_read);

    if (err != INFRA_OK) {
        if (err != INFRA_ERROR_WOULD_BLOCK) {
            INFRA_LOG_ERROR("Failed to read from socket: %d", err);
            conn->state = POLY_MUX_CONN_STATE_CLOSING;
        }
        return;
    }

    if (bytes_read == 0) {
        // 连接关闭
        conn->state = POLY_MUX_CONN_STATE_CLOSING;
        return;
    }

    conn->read_pos += bytes_read;
    conn->last_active = infra_time_ms();

    // 通知数据就绪
    if (conn->mux->events.on_data) {
        conn->mux->events.on_data(conn->mux->event_ctx, conn);
    }
}

// 处理可写事件
static void handle_writable(poly_mux_conn_t* conn) {
    if (!conn || !conn->sock || conn->write_pos == 0) return;

    // 发送数据
    size_t bytes_written = 0;
    infra_error_t err = infra_net_send(conn->sock,
        conn->write_buffer,
        conn->write_pos,
        &bytes_written);

    if (err != INFRA_OK) {
        if (err != INFRA_ERROR_WOULD_BLOCK) {
            INFRA_LOG_ERROR("Failed to write to socket: %d", err);
            conn->state = POLY_MUX_CONN_STATE_CLOSING;
        }
        return;
    }

    if (bytes_written > 0) {
        // 移动剩余数据
        if (bytes_written < conn->write_pos) {
            memmove(conn->write_buffer,
                conn->write_buffer + bytes_written,
                conn->write_pos - bytes_written);
        }
        conn->write_pos -= bytes_written;
        conn->last_active = infra_time_ms();

        // 通知可写就绪
        if (conn->write_pos == 0 && conn->mux->events.on_writable) {
            conn->mux->events.on_writable(conn->mux->event_ctx, conn);
        }
    }
}

// 处理连接完成
static void handle_connect_complete(poly_mux_conn_t* conn) {
    if (!conn || !conn->sock) return;

    // 检查连接结果
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(infra_net_get_fd(conn->sock), SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        error = errno;
    }

    if (error) {
        INFRA_LOG_ERROR("Connect failed: %d", error);
        conn->state = POLY_MUX_CONN_STATE_CLOSING;
        if (conn->mux->events.on_connect) {
            conn->mux->events.on_connect(conn->mux->event_ctx, conn, INFRA_ERROR_CONNECT_FAILED);
        }
        return;
    }

    // 连接成功
    conn->state = POLY_MUX_CONN_STATE_CONNECTED;
    if (conn->mux->events.on_connect) {
        conn->mux->events.on_connect(conn->mux->event_ctx, conn, INFRA_OK);
    }
}

// IO处理任务
static void* io_task(void* arg) {
    poly_mux_t* mux = (poly_mux_t*)arg;
    if (!mux) return NULL;

    while (mux->running) {
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        int max_fd = -1;

        // 设置文件描述符
        infra_mutex_lock(&mux->mutex);
        poly_mux_conn_t* conn = mux->conns;
        while (conn) {
            if (!conn->sock) {
                conn = conn->next;
                continue;
            }

            int fd = infra_net_get_fd(conn->sock);
            if (fd > max_fd) max_fd = fd;

            // 监听器总是可读
            if (conn->type == POLY_MUX_CONN_ACCEPT) {
                FD_SET(fd, &readfds);
            }
            // 正在连接的socket监听可写
            else if (conn->state == POLY_MUX_CONN_STATE_CONNECTING) {
                FD_SET(fd, &writefds);
            }
            // 已连接的socket根据状态监听
            else if (conn->state == POLY_MUX_CONN_STATE_CONNECTED) {
                FD_SET(fd, &readfds);
                if (conn->write_pos > 0) {
                    FD_SET(fd, &writefds);
                }
            }

            conn = conn->next;
        }
        infra_mutex_unlock(&mux->mutex);

        if (max_fd == -1) {
            infra_sleep(100);  // 100ms
            continue;
        }

        // 等待事件
        struct timeval tv = {.tv_sec = 0, .tv_usec = 100000};  // 100ms
        int ready = select(max_fd + 1, &readfds, &writefds, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            INFRA_LOG_ERROR("Select failed: %d", errno);
            break;
        }

        if (ready == 0) continue;

        // 处理事件
        uint64_t now = infra_time_ms();
        infra_mutex_lock(&mux->mutex);
        conn = mux->conns;
        poly_mux_conn_t* prev = NULL;
        while (conn) {
            poly_mux_conn_t* curr = conn;
            conn = conn->next;

            if (!curr->sock) {
                continue;
            }

            int fd = infra_net_get_fd(curr->sock);
            bool is_timeout = (now - curr->last_active) >= 
                (uint64_t)mux->config.conn_config.idle_timeout;

            // 检查超时
            if (is_timeout && curr->type != POLY_MUX_CONN_ACCEPT) {
                curr->state = POLY_MUX_CONN_STATE_CLOSING;
            }

            // 处理事件
            if (curr->state == POLY_MUX_CONN_STATE_CONNECTING && FD_ISSET(fd, &writefds)) {
                handle_connect_complete(curr);
            }
            else if (curr->state == POLY_MUX_CONN_STATE_CONNECTED) {
                if (FD_ISSET(fd, &readfds)) {
                    handle_readable(curr);
                }
                if (FD_ISSET(fd, &writefds)) {
                    handle_writable(curr);
                }
            }
            else if (curr->type == POLY_MUX_CONN_ACCEPT && FD_ISSET(fd, &readfds)) {
                // 接受新连接
                infra_socket_t client = NULL;
                infra_net_addr_t addr = {0};
                infra_error_t err = infra_net_accept(curr->sock, &client, &addr);
                if (err == INFRA_OK) {
                    if (mux->events.on_accept) {
                        mux->events.on_accept(mux->event_ctx, curr, &addr);
                    }
                } else if (err != INFRA_ERROR_WOULD_BLOCK) {
                    INFRA_LOG_ERROR("Failed to accept connection: %d", err);
                }
            }

            // 处理关闭状态
            if (curr->state == POLY_MUX_CONN_STATE_CLOSING) {
                remove_conn(mux, curr);
            } else {
                prev = curr;
            }
        }
        infra_mutex_unlock(&mux->mutex);
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

infra_error_t poly_mux_create(const poly_mux_config_t* config, poly_mux_t** mux) {
    if (!config || !mux) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配内存
    *mux = malloc(sizeof(poly_mux_t));
    if (!*mux) {
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(*mux, 0, sizeof(poly_mux_t));

    // 保存配置
    memcpy(&(*mux)->config, config, sizeof(poly_mux_config_t));

    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = config->min_threads,
        .max_threads = config->max_threads,
        .queue_size = config->queue_size
    };

    infra_error_t err = infra_thread_pool_create(&pool_config, &(*mux)->pool);
    if (err != INFRA_OK) {
        free(*mux);
        *mux = NULL;
        return err;
    }

    // 初始化互斥锁
    err = infra_mutex_create(&(*mux)->mutex);
    if (err != INFRA_OK) {
        infra_thread_pool_destroy((*mux)->pool);
        free(*mux);
        *mux = NULL;
        return err;
    }

    return INFRA_OK;
}

void poly_mux_destroy(poly_mux_t* mux) {
    if (!mux) return;

    // 停止服务
    poly_mux_stop(mux);

    // 等待任务完成
    while (true) {
        size_t active_threads, queued_tasks;
        infra_thread_pool_get_stats(mux->pool, &active_threads, &queued_tasks);
        if (active_threads == 0 && queued_tasks == 0) {
            break;
        }
        infra_sleep(10);  // 10ms
    }

    // 清理连接
    infra_mutex_lock(&mux->mutex);
    poly_mux_conn_t* conn = mux->conns;
    while (conn) {
        poly_mux_conn_t* next = conn->next;
        free_conn(conn);
        conn = next;
    }
    mux->conns = NULL;
    mux->curr_conns = 0;
    mux->total_conns = 0;
    infra_mutex_unlock(&mux->mutex);

    // 清理资源
    infra_mutex_destroy(&mux->mutex);
    infra_thread_pool_destroy(mux->pool);

    // 清理自身
    memset(mux, 0, sizeof(poly_mux_t));
    free(mux);
}

infra_error_t poly_mux_start(poly_mux_t* mux, const poly_mux_events_t* events, void* ctx) {
    if (!mux || !events) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mux->running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // 保存事件处理器
    memcpy(&mux->events, events, sizeof(poly_mux_events_t));
    mux->event_ctx = ctx;

    // 启动IO处理任务
    mux->running = true;
    infra_error_t err = infra_thread_pool_submit(mux->pool, io_task, mux);
    if (err != INFRA_OK) {
        mux->running = false;
        return err;
    }

    return INFRA_OK;
}

void poly_mux_stop(poly_mux_t* mux) {
    if (!mux || !mux->running) return;

    mux->running = false;

    // 等待任务完成
    while (true) {
        size_t active_threads, queued_tasks;
        infra_thread_pool_get_stats(mux->pool, &active_threads, &queued_tasks);
        if (active_threads == 0 && queued_tasks == 0) {
            break;
        }
        infra_sleep(10);  // 10ms
    }
}

infra_error_t poly_mux_listen(poly_mux_t* mux, const char* host, int port, poly_mux_conn_t** conn) {
    if (!mux || !host || !conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建监听socket
    infra_socket_t sock = NULL;
    infra_config_t config = {0};
    infra_error_t err = infra_net_create(&sock, false, &config);
    if (err != INFRA_OK) {
        return err;
    }

    // 设置地址重用
    err = infra_net_set_reuseaddr(sock, true);
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }

    // 绑定地址
    infra_net_addr_t addr = {
        .host = host,
        .port = port
    };
    err = infra_net_bind(sock, &addr);
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }

    // 开始监听
    err = infra_net_listen(sock);
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }

    // 创建连接对象
    poly_mux_conn_t* new_conn = create_conn(mux, sock, POLY_MUX_CONN_ACCEPT);
    if (!new_conn) {
        infra_net_close(sock);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 添加到连接列表
    infra_mutex_lock(&mux->mutex);
    new_conn->next = mux->conns;
    mux->conns = new_conn;
    mux->curr_conns++;
    mux->total_conns++;
    infra_mutex_unlock(&mux->mutex);

    *conn = new_conn;
    return INFRA_OK;
}

infra_error_t poly_mux_connect(poly_mux_t* mux, const char* host, int port, poly_mux_conn_t** conn) {
    if (!mux || !host || !conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建socket
    infra_socket_t sock = NULL;
    infra_config_t config = {0};
    infra_error_t err = infra_net_create(&sock, false, &config);
    if (err != INFRA_OK) {
        return err;
    }

    // 创建连接对象
    poly_mux_conn_t* new_conn = create_conn(mux, sock, POLY_MUX_CONN_CONNECT);
    if (!new_conn) {
        infra_net_close(sock);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置连接状态
    new_conn->state = POLY_MUX_CONN_STATE_CONNECTING;

    // 添加到连接列表
    infra_mutex_lock(&mux->mutex);
    new_conn->next = mux->conns;
    mux->conns = new_conn;
    mux->curr_conns++;
    mux->total_conns++;
    infra_mutex_unlock(&mux->mutex);

    // 开始连接
    infra_net_addr_t addr = {
        .host = host,
        .port = port
    };
    err = infra_net_connect(&addr, &sock, &config);
    if (err != INFRA_OK && err != INFRA_ERROR_WOULD_BLOCK) {
        remove_conn(mux, new_conn);
        return err;
    }

    *conn = new_conn;
    return INFRA_OK;
}

void poly_mux_conn_close(poly_mux_conn_t* conn) {
    if (!conn) return;
    conn->state = POLY_MUX_CONN_STATE_CLOSING;
}

ssize_t poly_mux_conn_read(poly_mux_conn_t* conn, void* buf, size_t size) {
    if (!conn || !buf || size == 0) return -1;

    // 检查是否有数据可读
    if (conn->read_pos == 0) return 0;

    // 确定实际读取大小
    size_t bytes = size;
    if (bytes > conn->read_pos) {
        bytes = conn->read_pos;
    }

    // 复制数据
    memcpy(buf, conn->read_buffer, bytes);

    // 移动剩余数据
    if (bytes < conn->read_pos) {
        memmove(conn->read_buffer,
            conn->read_buffer + bytes,
            conn->read_pos - bytes);
    }
    conn->read_pos -= bytes;

    return bytes;
}

ssize_t poly_mux_conn_write(poly_mux_conn_t* conn, const void* data, size_t size) {
    if (!conn || !data || size == 0) return -1;

    // 检查缓冲区空间
    size_t space = conn->mux->config.conn_config.write_buffer_size - conn->write_pos;
    if (space == 0) return 0;

    // 确定实际写入大小
    size_t bytes = size;
    if (bytes > space) {
        bytes = space;
    }

    // 复制数据
    memcpy(conn->write_buffer + conn->write_pos, data, bytes);
    conn->write_pos += bytes;

    return bytes;
}

poly_mux_conn_state_t poly_mux_conn_get_state(poly_mux_conn_t* conn) {
    return conn ? conn->state : POLY_MUX_CONN_STATE_NONE;
}

poly_mux_conn_type_t poly_mux_conn_get_type(poly_mux_conn_t* conn) {
    return conn ? conn->type : POLY_MUX_CONN_UNKNOWN;
}

void* poly_mux_conn_get_user_data(poly_mux_conn_t* conn) {
    return conn ? conn->user_data : NULL;
}

void poly_mux_conn_set_user_data(poly_mux_conn_t* conn, void* data) {
    if (conn) conn->user_data = data;
}

infra_error_t poly_mux_get_stats(poly_mux_t* mux, uint32_t* curr_conns, uint32_t* total_conns) {
    if (!mux || !curr_conns || !total_conns) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mutex_lock(&mux->mutex);
    *curr_conns = mux->curr_conns;
    *total_conns = mux->total_conns;
    infra_mutex_unlock(&mux->mutex);

    return INFRA_OK;
} 

