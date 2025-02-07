#include "internal/infrax/InfraxAsync.h"
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>

// 状态定义
#define ASYNC_STATE_INIT    0  // 初始状态
#define ASYNC_STATE_RUNNING 1  // 正在运行
#define ASYNC_STATE_YIELD   2  // 已让出控制权
#define ASYNC_STATE_DONE    3  // 已完成
#define ASYNC_STATE_ERROR   4  // 出错

struct InfraxAsync {
    jmp_buf env;           // 保存的执行环境
    AsyncFn fn;            // 异步函数
    void* arg;             // 函数参数
    int pipe_fd[2];        // 用于通知完成
    int error;             // 错误代码
    int state;             // 当前状态
    struct InfraxAsync* next;    // 下一个任务
    struct InfraxAsync* parent;  // 父任务（谁在等待我）
};

// 让出控制权给调用者
void infrax_async_yield(InfraxAsync* async) {
    if (!async) return;
    
    // 找到父任务
    InfraxAsync* parent = async->parent;
    if (!parent) return;
    
    // 更新状态
    async->state = ASYNC_STATE_YIELD;
    
    // 跳回到父任务的等待点
    longjmp(parent->env, 1);
}

InfraxAsync* infrax_async_start(AsyncFn fn, void* arg) {
    InfraxAsync* async = (InfraxAsync*)malloc(sizeof(InfraxAsync));
    if (!async) return NULL;
    
    // 初始化
    async->fn = fn;
    async->arg = arg;
    async->error = 0;
    async->state = ASYNC_STATE_INIT;
    async->next = NULL;
    async->parent = NULL;
    
    // 创建通知管道
    if (pipe(async->pipe_fd) != 0) {
        free(async);
        return NULL;
    }
    
    return async;
}

int infrax_async_wait(InfraxAsync* async) {
    if (!async) return -1;
    
    // 设置父子关系
    async->parent = async;  // 指向自己表示这是根任务
    
    // 执行任务
    while (async->state != ASYNC_STATE_DONE && 
           async->state != ASYNC_STATE_ERROR) {
        
        if (setjmp(async->env) == 0) {
            // 第一次进入或从 yield 恢复
            if (async->state != ASYNC_STATE_DONE) {
                async->state = ASYNC_STATE_RUNNING;
                async->fn(async->arg);
                async->state = ASYNC_STATE_DONE;
            }
        }
        
        usleep(1000); // 避免过度消耗 CPU
    }
    
    // 发送完成通知
    char done = 1;
    write(async->pipe_fd[1], &done, 1);
    
    // 等待完成通知
    read(async->pipe_fd[0], &done, 1);
    
    // 清理
    int error = async->error;
    close(async->pipe_fd[0]);
    close(async->pipe_fd[1]);
    free(async);
    
    return error;
}
