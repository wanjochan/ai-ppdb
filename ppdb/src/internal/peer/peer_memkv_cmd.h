#ifndef PEER_MEMKV_CMD_H
#define PEER_MEMKV_CMD_H

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

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 解析命令
infra_error_t memkv_parse_command(memkv_conn_t* conn);

// 执行命令
infra_error_t memkv_execute_command(memkv_conn_t* conn);

// 发送响应
infra_error_t memkv_send_response(memkv_conn_t* conn, const char* fmt, ...);

#endif // PEER_MEMKV_CMD_H
