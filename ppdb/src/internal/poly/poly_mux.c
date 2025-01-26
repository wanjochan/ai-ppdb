#include "internal/poly/poly_mux.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_platform.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 连接结构
typedef struct poly_mux_conn {
    infra_socket_t sock;           // 客户端socket
    struct poly_mux* mux;          // 所属多路复用器
    infra_time_t last_active;      // 最后活跃时间
    struct poly_mux_conn* next;    // 链表下一项
} poly_mux_conn_t;

// 多路复用器实现
struct poly_mux {
    bool running;                  // 运行标志
    infra_socket_t listener;       // 监听socket
    poly_mux_config_t config;      // 配置信息
    infra_thread_pool_t* pool;     // 线程池
    infra_mutex_t mutex;           // 全局互斥锁
    poly_mux_handler_t handler;    // 连接处理回调
    void* handler_ctx;             // 回调上下文
    poly_mux_conn_t* conns;        // 活跃连接链表
    size_t curr_conns;             // 当前连接数
    size_t total_conns;            // 总连接数
};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 创建连接
static poly_mux_conn_t* create_conn(poly_mux_t* mux, infra_socket_t sock) {
    poly_mux_conn_t* conn = malloc(sizeof(poly_mux_conn_t));
    if (!conn) {
        return NULL;
    }

    conn->sock = sock;
    conn->mux = mux;
    conn->last_active = infra_time_monotonic();  // 使用单调时间
    conn->next = NULL;

    return conn;
}

// 销毁连接
static void destroy_conn(poly_mux_conn_t* conn) {
    if (!conn) {
        return;
    }

    if (conn->sock) {
        infra_net_shutdown(conn->sock, INFRA_NET_SHUTDOWN_BOTH);
        infra_net_close(conn->sock);
    }

    free(conn);
}

// 添加连接
static infra_error_t add_conn(poly_mux_t* mux, poly_mux_conn_t* conn) {
    infra_mutex_lock(&mux->mutex);

    // 检查连接数限制
    if (mux->curr_conns >= mux->config.max_connections) {
        infra_mutex_unlock(&mux->mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 添加到链表头部
    conn->next = mux->conns;
    mux->conns = conn;
    mux->curr_conns++;
    mux->total_conns++;

    infra_mutex_unlock(&mux->mutex);
    return INFRA_OK;
}

// 移除连接
static void remove_conn(poly_mux_t* mux, poly_mux_conn_t* conn) {
    infra_mutex_lock(&mux->mutex);

    if (mux->conns == conn) {
        mux->conns = conn->next;
    } else {
        poly_mux_conn_t* prev = mux->conns;
        while (prev && prev->next != conn) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = conn->next;
        }
    }

    mux->curr_conns--;

    infra_mutex_unlock(&mux->mutex);
}

// 清理超时连接
static void cleanup_idle_conns(poly_mux_t* mux) {
    infra_mutex_lock(&mux->mutex);

    infra_time_t now = infra_time_monotonic();  // 使用单调时间
    poly_mux_conn_t* curr = mux->conns;
    poly_mux_conn_t* prev = NULL;

    while (curr) {
        if (now - curr->last_active > mux->config.idle_timeout) {
            poly_mux_conn_t* to_remove = curr;
            curr = curr->next;

            if (prev) {
                prev->next = curr;
            } else {
                mux->conns = curr;
            }

            mux->curr_conns--;
            destroy_conn(to_remove);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    infra_mutex_unlock(&mux->mutex);
}

// 连接处理任务
static void* handle_conn_task(void* arg) {
    poly_mux_conn_t* conn = (poly_mux_conn_t*)arg;
    if (!conn || !conn->mux) {
        return NULL;
    }

    poly_mux_t* mux = conn->mux;

    // 处理连接
    infra_error_t err = mux->handler(mux->handler_ctx, conn->sock);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to handle connection: %d", err);
        // 只在处理失败时移除并销毁连接
        remove_conn(mux, conn);
        destroy_conn(conn);
    }

    return NULL;
}

// 接受连接任务
static void* accept_conn_task(void* arg) {
    poly_mux_t* mux = (poly_mux_t*)arg;
    if (!mux) {
        return NULL;
    }

    while (mux->running) {
        // 接受新连接
        infra_socket_t client = NULL;
        infra_net_addr_t addr = {0};
        infra_error_t err = infra_net_accept(mux->listener, &client, &addr);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                infra_sleep(10);  // 10ms
                continue;
            }
            INFRA_LOG_ERROR("Failed to accept connection: %d", err);
            break;
        }

        INFRA_LOG_INFO("Accepted connection from %s:%d", addr.host, addr.port);

        // 设置客户端socket为非阻塞模式
        err = infra_net_set_nonblock(client, true);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to set client socket non-blocking: %d", err);
            infra_net_close(client);
            continue;
        }

        // 创建连接结构
        poly_mux_conn_t* conn = create_conn(mux, client);
        if (!conn) {
            INFRA_LOG_ERROR("Failed to create connection");
            infra_net_close(client);
            continue;
        }

        // 添加到连接列表
        err = add_conn(mux, conn);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to add connection: %d", err);
            destroy_conn(conn);
            continue;
        }

        // 提交到线程池处理
        err = infra_thread_pool_submit(mux->pool, handle_conn_task, conn);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to submit connection task: %d", err);
            remove_conn(mux, conn);
            destroy_conn(conn);
            continue;
        }

        // 清理空闲连接
        cleanup_idle_conns(mux);
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// Interface Implementation
//-----------------------------------------------------------------------------

infra_error_t poly_mux_create(const poly_mux_config_t* config, poly_mux_t** mux) {
    if (!config || !mux) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建多路复用器
    poly_mux_t* m = malloc(sizeof(poly_mux_t));
    if (!m) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化字段
    memset(m, 0, sizeof(poly_mux_t));
    memcpy(&m->config, config, sizeof(poly_mux_config_t));

    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = config->min_threads,
        .max_threads = config->max_threads,
        .queue_size = config->queue_size
    };

    infra_error_t err = infra_thread_pool_create(&pool_config, &m->pool);
    if (err != INFRA_OK) {
        free(m);
        return err;
    }

    // 创建互斥锁
    err = infra_mutex_create(&m->mutex);
    if (err != INFRA_OK) {
        infra_thread_pool_destroy(m->pool);
        free(m);
        return err;
    }

    *mux = m;
    return INFRA_OK;
}

void poly_mux_destroy(poly_mux_t* mux) {
    if (!mux) {
        return;
    }

    // 停止服务
    if (mux->running) {
        poly_mux_stop(mux);
    }

    // 清理连接
    while (mux->conns) {
        poly_mux_conn_t* conn = mux->conns;
        mux->conns = conn->next;
        destroy_conn(conn);
    }

    // 清理资源
    if (mux->pool) {
        infra_thread_pool_destroy(mux->pool);
    }
    infra_mutex_destroy(&mux->mutex);
    free(mux);
}

infra_error_t poly_mux_start(poly_mux_t* mux, poly_mux_handler_t handler, void* ctx) {
    if (!mux || !handler) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (mux->running) {
        return INFRA_ERROR_BUSY;
    }

    // 创建监听socket
    infra_socket_t listener = NULL;
    infra_config_t config = {0};
    infra_error_t err = infra_net_create(&listener, false, &config);
    if (err != INFRA_OK) {
        return err;
    }

    // 设置地址重用
    err = infra_net_set_reuseaddr(listener, true);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    // 设置非阻塞
    err = infra_net_set_nonblock(listener, true);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    // 绑定地址
    infra_net_addr_t addr = {0};
    addr.host = mux->config.host;
    addr.port = mux->config.port > 0 ? mux->config.port : 11211;  // 使用默认端口11211
    err = infra_net_bind(listener, &addr);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    // 获取实际绑定的端口号
    infra_net_addr_t bound_addr = {0};
    err = infra_net_getsockname(listener, &bound_addr);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }
    mux->config.port = bound_addr.port;

    // 开始监听
    err = infra_net_listen(listener);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    // 保存状态
    mux->listener = listener;
    mux->handler = handler;
    mux->handler_ctx = ctx;
    mux->running = true;

    // 启动接受连接任务
    err = infra_thread_pool_submit(mux->pool, accept_conn_task, mux);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        mux->listener = NULL;
        mux->running = false;
        return err;
    }

    INFRA_LOG_INFO("Multiplexer started on %s:%d", bound_addr.host, bound_addr.port);
    return INFRA_OK;
}

infra_error_t poly_mux_stop(poly_mux_t* mux) {
    if (!mux) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!mux->running) {
        return INFRA_OK;
    }

    // 停止服务
    mux->running = false;

    // 关闭监听socket
    if (mux->listener) {
        infra_net_close(mux->listener);
        mux->listener = NULL;
    }

    // 等待所有任务完成
    infra_thread_pool_destroy(mux->pool);
    mux->pool = NULL;

    INFRA_LOG_INFO("Multiplexer stopped");
    return INFRA_OK;
}

infra_error_t poly_mux_get_stats(poly_mux_t* mux, size_t* curr_conns, size_t* total_conns) {
    if (!mux || !curr_conns || !total_conns) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mutex_lock(&mux->mutex);
    *curr_conns = mux->curr_conns;
    *total_conns = mux->total_conns;
    infra_mutex_unlock(&mux->mutex);

    return INFRA_OK;
}

bool poly_mux_is_running(const poly_mux_t* mux) {
    return mux ? mux->running : false;
}

infra_socket_t poly_mux_get_listener(const poly_mux_t* mux) {
    return mux ? mux->listener : NULL;
} 