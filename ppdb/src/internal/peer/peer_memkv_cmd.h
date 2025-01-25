#ifndef PEER_MEMKV_CMD_H
#define PEER_MEMKV_CMD_H

#include "internal/infra/infra_core.h"
#include "internal/peer/peer_memkv.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION "1.0.0"
#define MEMKV_MAX_TOKENS 8
#define MEMKV_MAX_CMD_LEN 256

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 命令处理函数类型
typedef infra_error_t (*memkv_cmd_handler_fn)(memkv_conn_t* conn);

// 命令处理器
typedef struct {
    const char* name;           // 命令名
    memkv_cmd_type_t type;     // 命令类型
    memkv_cmd_handler_fn fn;   // 处理函数
    int min_tokens;            // 最少参数数量
    int max_tokens;            // 最多参数数量
    bool need_data;            // 是否需要数据块
} memkv_cmd_handler_t;

// Meta 命令标志位
#define META_FLAG_BINARY_KEY    (1 << 0)  // 二进制 key
#define META_FLAG_BATCH         (1 << 1)  // 批量操作
#define META_FLAG_CONDITION     (1 << 2)  // 条件操作
#define META_FLAG_ATOMIC        (1 << 3)  // 原子操作

// Meta 命令条件类型
typedef enum {
    META_COND_NONE = 0,
    META_COND_EXISTS,         // key 存在
    META_COND_NOT_EXISTS,     // key 不存在
    META_COND_EQUAL,         // 值等于
    META_COND_NOT_EQUAL,     // 值不等于
    META_COND_LESS,          // 值小于
    META_COND_LESS_EQUAL,    // 值小于等于
    META_COND_GREATER,       // 值大于
    META_COND_GREATER_EQUAL, // 值大于等于
} meta_condition_t;

// Meta 命令结构
typedef struct {
    uint32_t flags;           // 标志位
    meta_condition_t cond;    // 条件类型
    char* cond_value;        // 条件值
    size_t cond_value_len;   // 条件值长度
    char** keys;             // 键列表（批量操作用）
    size_t key_count;        // 键数量
} meta_cmd_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 解析命令
infra_error_t memkv_parse_command(memkv_conn_t* conn);

// 执行命令
infra_error_t memkv_execute_command(memkv_conn_t* conn);

// 发送响应
infra_error_t memkv_send_response(memkv_conn_t* conn, const char* fmt, ...);

// Helper functions
void destroy_item(memkv_item_t* item);

// Command handlers
infra_error_t handle_set(memkv_conn_t* conn);
infra_error_t handle_get(memkv_conn_t* conn);
infra_error_t handle_delete(memkv_conn_t* conn);
infra_error_t handle_flush_all(memkv_conn_t* conn);

//-----------------------------------------------------------------------------
// Command Processing
//-----------------------------------------------------------------------------

/**
 * Initialize the command processing module.
 * 
 * @return INFRA_OK on success, error code otherwise.
 */
infra_error_t memkv_cmd_init(void);

/**
 * Clean up the command processing module.
 * 
 * @return INFRA_OK on success, error code otherwise.
 */
infra_error_t memkv_cmd_cleanup(void);

/**
 * Process commands from a connection.
 * 
 * @param conn The connection to process commands from.
 * @return INFRA_OK on success, INFRA_ERROR_WOULD_BLOCK if more data is needed,
 *         or error code on failure.
 */
infra_error_t memkv_cmd_process(memkv_conn_t* conn);

#endif // PEER_MEMKV_CMD_H
