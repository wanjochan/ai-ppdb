#ifndef PEER_MEMKV_H
#define PEER_MEMKV_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_platform.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/poly/poly_cmdline.h"

// 命令类型
typedef enum {
    CMD_UNKNOWN = 0,
    CMD_SET,
    CMD_ADD,
    CMD_REPLACE,
    CMD_APPEND,
    CMD_PREPEND,
    CMD_CAS,
    CMD_GET,
    CMD_GETS,
    CMD_INCR,
    CMD_DECR,
    CMD_TOUCH,
    CMD_GAT,
    CMD_FLUSH,
    CMD_DELETE,
    CMD_STATS,
    CMD_VERSION,
    CMD_QUIT,
    CMD_MAX
} memkv_cmd_type_t;

// 错误码
#define INFRA_ERROR_BUFFER_FULL      -100
#define INFRA_ERROR_CLIENT_ERROR     -101

#define MEMKV_BUFFER_SIZE 16384      // 命令缓冲区大小
#define MEMKV_MAX_KEY_SIZE 250
#define MEMKV_MAX_VALUE_SIZE (1024 * 1024)  // 1MB
#define MEMKV_MAX_CONNECTIONS 10000
#define MEMKV_DEFAULT_PORT 11211

// 线程池配置
#define MEMKV_MIN_THREADS 32           // 最小线程数
#define MEMKV_MAX_THREADS 512          // 最大线程数
#define MEMKV_QUEUE_SIZE 1000          // 任务队列大小
#define MEMKV_IDLE_TIMEOUT 60000       // 空闲线程超时时间(ms)

// 统计信息
typedef struct memkv_stats {
    uint64_t cmd_get;        // get 命令次数
    uint64_t cmd_set;        // set 命令次数
    uint64_t cmd_delete;     // delete 命令次数
    uint64_t hits;           // 缓存命中次数
    uint64_t misses;         // 缓存未命中次数
    uint64_t curr_items;     // 当前项数量
    uint64_t total_items;    // 总项数量
    uint64_t bytes;          // 当前使用的字节数
} memkv_stats_t;

// 存储项
typedef struct memkv_item {
    char* key;                    // 键
    void* value;                  // 值
    size_t value_size;           // 值大小
    uint32_t flags;              // 标志位
    uint32_t exptime;            // 过期时间
    uint64_t cas;                // CAS值
} memkv_item_t;

// 全局上下文
typedef struct memkv_context {
    bool running;                  // 服务运行状态
    uint16_t port;                // 监听端口
    infra_socket_t listener;      // 监听套接字
    infra_mutex_t store_mutex;    // 存储锁
    infra_thread_pool_t* pool;    // 线程池
    memkv_stats_t stats;          // 统计信息
    poly_hashtable_t* store;      // 存储哈希表
} memkv_context_t;

// 客户端连接
typedef struct memkv_conn {
    infra_socket_t socket;        // 客户端socket
    char* buffer;                 // 命令缓冲区
    size_t buffer_size;          // 缓冲区大小
    size_t buffer_used;          // 已使用大小
    memkv_cmd_type_t cmd_type;   // 命令类型
    void* data;                  // 命令数据
    size_t data_size;           // 数据大小
    size_t data_remaining;      // 剩余数据大小
    char* key;                  // 当前命令的键
    uint32_t flags;            // 标志位
    uint32_t exptime;          // 过期时间
    uint64_t cas;              // CAS值
} memkv_conn_t;

// 接口函数
infra_error_t memkv_init(uint16_t port);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
infra_error_t memkv_cleanup(void);

// 辅助函数声明
memkv_item_t* create_item(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime);
void destroy_item(memkv_item_t* item);
bool is_item_expired(const memkv_item_t* item);
void update_stats_set(size_t value_size);
void update_stats_get(bool hit);
void update_stats_delete(size_t value_size);

#endif // PEER_MEMKV_H
