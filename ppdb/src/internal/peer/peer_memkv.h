#ifndef PEER_MEMKV_H
#define PEER_MEMKV_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/poly/poly_cmdline.h"
#include "internal/poly/poly_hashtable.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_BUFFER_SIZE 16384        // 命令缓冲区大小
#define MEMKV_MAX_KEY_SIZE 250         // 键最大长度
#define MEMKV_MAX_VALUE_SIZE (1024*1024) // 值最大长度 1MB
#define MEMKV_MIN_THREADS 32           // 最小线程数
#define MEMKV_MAX_THREADS 512          // 最大线程数

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 统计信息
typedef struct {
    infra_atomic_t cmd_get;        // get 命令次数
    infra_atomic_t cmd_set;        // set 命令次数
    infra_atomic_t cmd_delete;     // delete 命令次数
    infra_atomic_t hits;           // 缓存命中次数
    infra_atomic_t misses;         // 缓存未命中次数
    infra_atomic_t curr_items;     // 当前项数量
    infra_atomic_t total_items;    // 总项数量
    infra_atomic_t bytes;          // 当前使用的字节数
} memkv_stats_t;

// 存储项
typedef struct {
    char* key;                  // 键（字符串）
    void* value;               // 值（二进制数据）
    size_t value_size;         // 值大小
    uint32_t flags;            // Memcached flags
    time_t exptime;            // 过期时间（0表示永不过期）
    uint64_t cas;              // CAS标识符
} memkv_item_t;

// 命令解析状态
typedef enum {
    PARSE_STATE_INIT,          // 初始状态
    PARSE_STATE_DATA,          // 等待数据
    PARSE_STATE_COMPLETE       // 命令完成
} memkv_parse_state_t;

// 命令类型
typedef enum {
    CMD_UNKNOWN,
    CMD_GET,
    CMD_SET,
    CMD_DELETE,
    CMD_STATS,
    CMD_VERSION,
    CMD_QUIT
} memkv_cmd_type_t;

// 命令结构
typedef struct {
    memkv_cmd_type_t type;    // 命令类型
    char* key;                // 键
    void* data;               // 数据（用于set）
    size_t data_len;          // 数据长度
    uint32_t flags;           // 标志位
    time_t exptime;           // 过期时间
    size_t bytes;             // 数据字节数（用于set）
    uint64_t cas;             // CAS值
} memkv_cmd_t;

// 客户端连接
typedef struct {
    infra_socket_t socket;     // 客户端socket
    char* buffer;              // 命令缓冲区
    size_t buffer_size;        // 缓冲区大小
    size_t buffer_used;        // 缓冲区已用大小
    memkv_parse_state_t state; // 解析状态
    memkv_cmd_t current_cmd;   // 当前命令
    size_t data_remaining;     // 剩余数据长度（用于set）
    infra_mutex_t mutex;       // 互斥锁
} memkv_conn_t;

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

extern const poly_cmd_option_t memkv_options[];
extern const int memkv_option_count;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 命令处理函数
infra_error_t memkv_cmd_handler(int argc, char** argv);

// 初始化KV存储
infra_error_t memkv_init(const infra_config_t* config);

// 清理KV存储
infra_error_t memkv_cleanup(void);

// 启动服务
infra_error_t memkv_start(void);

// 停止服务
infra_error_t memkv_stop(void);

// 检查服务状态
bool memkv_is_running(void);

// 获取统计信息
const memkv_stats_t* memkv_get_stats(void);

#endif // PEER_MEMKV_H
