#ifndef PEER_MEMKV_H
#define PEER_MEMKV_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_platform.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/poly/poly_cmdline.h"
#include "../poly_atomic/poly_atomic.h"

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

// 命令类型
typedef enum memkv_cmd_type {
    CMD_UNKNOWN = 0,
    CMD_SET,        // SET key flags exptime bytes [noreply]\r\n
    CMD_GET,        // GET key [key...]\r\n
    CMD_ADD,        // ADD key flags exptime bytes [noreply]\r\n
    CMD_REPLACE,    // REPLACE key flags exptime bytes [noreply]\r\n
    CMD_DELETE,     // DELETE key [noreply]\r\n
    CMD_INCR,       // INCR key value [noreply]\r\n
    CMD_DECR,       // DECR key value [noreply]\r\n
    CMD_QUIT,       // QUIT\r\n
    CMD_VERSION,    // VERSION\r\n
    CMD_STATS,      // STATS\r\n
    CMD_APPEND,     // APPEND key flags exptime bytes [noreply]\r\n
    CMD_PREPEND,    // PREPEND key flags exptime bytes [noreply]\r\n
    CMD_CAS,        // CAS key flags exptime bytes cas [noreply]\r\n
    CMD_GETS,       // GETS key [key...]\r\n
    CMD_TOUCH,      // TOUCH key exptime [noreply]\r\n
    CMD_GAT,        // GAT exptime key\r\n
    CMD_FLUSH       // FLUSH_ALL [exptime] [noreply]\r\n
} memkv_cmd_type_t;

// 命令状态
typedef enum memkv_cmd_state {
    CMD_STATE_INIT,         // 初始状态
    CMD_STATE_READ_DATA,    // 读取数据
    CMD_STATE_EXECUTING,    // 执行中
    CMD_STATE_COMPLETE      // 完成
} memkv_cmd_state_t;

// 命令结构
typedef struct memkv_cmd {
    memkv_cmd_type_t type;     // 命令类型
    memkv_cmd_state_t state;   // 命令状态
    char* key;                 // 键
    size_t key_len;           // 键长度
    void* data;               // 数据
    size_t bytes;             // 数据长度
    size_t bytes_read;        // 已读取的数据长度
    uint32_t flags;           // 标志位
    uint32_t exptime;         // 过期时间
    uint64_t cas;             // CAS值
    bool noreply;             // 是否不需要回复
} memkv_cmd_t;

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

// 连接结构
typedef struct memkv_conn {
    infra_socket_t sock;              // 套接字
    char buffer[MEMKV_BUFFER_SIZE];   // 命令缓冲区
    size_t buffer_used;               // 已使用的缓冲区大小
    size_t buffer_read;               // 已读取的缓冲区大小
    memkv_cmd_t current_cmd;          // 当前命令
    bool is_active;                   // 连接是否活跃
} memkv_conn_t;

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
} memkv_context_t;

// 声明全局上下文
extern memkv_context_t g_context;

// 函数声明
infra_error_t send_response(memkv_conn_t* conn, const char* data, size_t len);
infra_error_t poly_hashtable_delete(poly_hashtable_t* table, const char* key);

// 接口函数
infra_error_t memkv_init(uint16_t port);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
bool memkv_is_running(void);
const memkv_stats_t* memkv_get_stats(void);

// Helper functions
memkv_item_t* create_item(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime);
void destroy_item(memkv_item_t* item);
bool is_item_expired(const memkv_item_t* item);
void update_stats_set(size_t value_size);
void update_stats_get(bool hit);
void update_stats_delete(size_t value_size);

#endif // PEER_MEMKV_H
