#ifndef PPDB_H
#define PPDB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//-----------------------------------------------------------------------------
// 基础定义
//-----------------------------------------------------------------------------

// 错误码
typedef int32_t ppdb_error_t;

// 常见错误
#define PPDB_OK                     0  // 成功
#define PPDB_ERR_NULL_POINTER       1  // 空指针
#define PPDB_ERR_INVALID_ARGUMENT   2  // 无效参数
#define PPDB_ERR_INVALID_STATE      3  // 无效状态
#define PPDB_ERR_NOT_IMPLEMENTED    4  // 未实现
#define PPDB_ERR_OUT_OF_MEMORY     5  // 内存不足
#define PPDB_ERR_TIMEOUT           6  // 超时
#define PPDB_ERR_BUSY             7  // 忙
#define PPDB_ERR_FULL             8  // 满
#define PPDB_ERR_NOT_FOUND        9  // 未找到
#define PPDB_ERR_EXISTS           10 // 已存在

// 句柄类型
typedef uint64_t ppdb_handle_t;    // 基础句柄类型
typedef ppdb_handle_t ppdb_ctx_t;  // 上下文句柄
typedef ppdb_handle_t ppdb_db_t;   // 数据库句柄
typedef ppdb_handle_t ppdb_tx_t;   // 事务句柄

// 数据缓冲区
typedef struct ppdb_data {
    uint8_t inline_data[32];  // 小数据优化
    uint32_t size;           // 数据大小
    uint32_t flags;          // 标志位
    void* extended_data;     // 大数据指针
} ppdb_data_t;

//-----------------------------------------------------------------------------
// 数据库配置
//-----------------------------------------------------------------------------

typedef struct ppdb_options {
    const char* db_path;          // 数据库路径
    uint64_t cache_size;         // 缓存大小
    uint32_t max_readers;        // 最大读取器数量
    bool sync_writes;           // 同步写入
    uint32_t flush_period_ms;   // 刷新周期
} ppdb_options_t;

//-----------------------------------------------------------------------------
// 数据库接口
//-----------------------------------------------------------------------------

// 创建数据库上下文
ppdb_error_t ppdb_create(ppdb_ctx_t* ctx, const ppdb_options_t* options);

// 销毁数据库上下文
ppdb_error_t ppdb_destroy(ppdb_ctx_t ctx);

// 打开数据库
ppdb_error_t ppdb_open(ppdb_ctx_t ctx, ppdb_db_t* db, const char* name);

// 关闭数据库
ppdb_error_t ppdb_close(ppdb_db_t db);

// 开始事务
ppdb_error_t ppdb_begin_tx(ppdb_db_t db, ppdb_tx_t* tx, bool read_only);

// 提交事务
ppdb_error_t ppdb_commit_tx(ppdb_tx_t tx);

// 回滚事务
ppdb_error_t ppdb_rollback_tx(ppdb_tx_t tx);

// 写入数据
ppdb_error_t ppdb_put(ppdb_tx_t tx, const ppdb_data_t* key, const ppdb_data_t* value);

// 读取数据
ppdb_error_t ppdb_get(ppdb_tx_t tx, const ppdb_data_t* key, ppdb_data_t* value);

// 删除数据
ppdb_error_t ppdb_delete(ppdb_tx_t tx, const ppdb_data_t* key);

// 清空数据库
ppdb_error_t ppdb_clear(ppdb_tx_t tx);

// 获取统计信息
ppdb_error_t ppdb_get_stats(ppdb_db_t db, char* buffer, size_t size);

#endif // PPDB_H
