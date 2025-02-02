#ifndef POLY_ASYNC_H_
#define POLY_ASYNC_H_

#include <stddef.h>
#include <stdbool.h>

// 异步操作类型
typedef enum {
    POLY_ASYNC_OP_NONE = 0,
    POLY_ASYNC_OP_WAIT,      // 等待事件
    POLY_ASYNC_OP_TIMEOUT,   // 超时
    POLY_ASYNC_OP_SIGNAL,    // 信号
    POLY_ASYNC_OP_CUSTOM     // 用户自定义
} poly_async_op_t;

// 异步操作结果
typedef struct {
    int status;     // 0表示成功，负数表示错误
    size_t bytes;   // 传输的字节数（如果适用）
} poly_async_result_t;

// 异步上下文（不暴露内部实现）
typedef struct poly_async_context poly_async_context_t;

/**
 * @brief 创建异步上下文
 * @return 上下文指针，失败返回NULL
 */
poly_async_context_t* poly_async_create(void);

/**
 * @brief 销毁异步上下文
 * @param ctx 上下文指针
 */
void poly_async_destroy(poly_async_context_t* ctx);

/**
 * @brief 等待异步操作完成
 * @param ctx 上下文指针
 * @param op 操作类型
 * @param timeout_ms 超时时间（毫秒），-1表示永不超时
 * @return 异步操作结果
 */
poly_async_result_t poly_async_wait(poly_async_context_t* ctx, 
                                  poly_async_op_t op,
                                  int timeout_ms);

/**
 * @brief 运行异步事件循环
 * @param ctx 上下文指针
 * @return 成功返回0，失败返回-1
 */
int poly_async_run(poly_async_context_t* ctx);
