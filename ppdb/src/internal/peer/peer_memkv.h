#ifndef PEER_MEMKV_H
#define PEER_MEMKV_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_platform.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/poly/poly_cmdline.h"
#include "../poly/poly_atomic.h"

// 前向声明
struct memkv_conn;
typedef struct memkv_conn memkv_conn_t;

// 常量定义
#define MEMKV_VERSION          "1.0.0"
#define MEMKV_BUFFER_SIZE      16384
#define MEMKV_MAX_KEY_SIZE     250
#define MEMKV_MAX_VALUE_SIZE   (1024 * 1024)  // 1MB
#define MEMKV_MAX_CONNECTIONS  10000
#define MEMKV_DEFAULT_PORT     11211

// 线程池配置
#define MEMKV_MIN_THREADS      4
#define MEMKV_MAX_THREADS      32
#define MEMKV_QUEUE_SIZE       1000
#define MEMKV_IDLE_TIMEOUT     60000  // 60秒

// 错误码定义
#define MEMKV_OK                  0
#define MEMKV_ERROR_NO_MEMORY    -1
#define MEMKV_ERROR_NOT_FOUND    -2
#define MEMKV_ERROR_EXISTS       -3
#define MEMKV_ERROR_INVALID      -4
#define MEMKV_ERROR_BUSY         -5
#define MEMKV_ERROR_CLIENT_ERROR -6
#define MEMKV_ERROR_BUFFER_FULL  -7

// 存储项
typedef struct memkv_item {
    char* key;                // 键
    void* value;             // 值
    size_t value_size;       // 值大小
    uint32_t flags;          // 标志位
    uint32_t exptime;        // 过期时间
    uint64_t cas;            // CAS值
    time_t ctime;            // 创建时间
    time_t atime;            // 最后访问时间
} memkv_item_t;

// 统计信息
typedef struct memkv_stats {
    poly_atomic_t cmd_get;        // GET命令次数
    poly_atomic_t cmd_set;        // SET命令次数
    poly_atomic_t cmd_delete;     // DELETE命令次数
    poly_atomic_t hits;           // 缓存命中次数
    poly_atomic_t misses;         // 缓存未命中次数
    poly_atomic_t curr_items;     // 当前项数量
    poly_atomic_t total_items;    // 总项数量
    poly_atomic_t bytes;          // 总字节数
} memkv_stats_t;

// 全局上下文
typedef struct memkv_context {
    bool is_running;                // 服务运行状态
    uint16_t port;                 // 监听端口
    infra_socket_t listen_sock;    // 监听套接字
    infra_thread_pool_t* pool;     // 线程池
    infra_mutex_t store_mutex;     // 存储互斥锁
    poly_hashtable_t* store;       // 存储哈希表
    memkv_stats_t stats;           // 统计信息
    time_t start_time;             // 启动时间
    infra_thread_t* accept_thread;  // 接受连接的线程
    uint64_t next_cas;             // 用于生成唯一的 CAS 值
} memkv_context_t;

// 声明全局上下文
extern memkv_context_t g_context;

// 公共接口函数
infra_error_t memkv_init(uint16_t port, const infra_config_t* config);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
bool memkv_is_running(void);
const memkv_stats_t* memkv_get_stats(void);

#endif // PEER_MEMKV_H
