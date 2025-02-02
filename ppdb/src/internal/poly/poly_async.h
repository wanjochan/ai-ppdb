#pragma once
#include <setjmp.h>
#include <stddef.h>

// 栈大小配置
#define POLY_STACK_MIN  (4 * 1024)     // 4KB 起始栈
#define POLY_STACK_MAX  (1024 * 1024)  // 1MB 上限

// 默认栈大小 64KB
// #define POLY_STACK_SIZE (64 * 1024)
/*
64位系统：
128TB / 65.5KB ≈ 2,000,000,000 个协程（理论值）

实际限制因素：
1. 可用物理内存（如16GB）：
   16GB / 65.5KB  约≈ 250,000 个协程

2. 操作系统限制：
   - 默认进程虚拟内存限制
   - 默认进程最大内存映射区域数
*/
// 协程函数类型
typedef void (*poly_async_fn)(void*);

// 协程上下文
typedef struct poly_async_ctx {
    jmp_buf env;           // 上下文
    char* stack;           // 栈指针
    size_t size;          // 当前栈大小
    size_t used;          // 已用空间
    poly_async_fn fn;      // 协程函数
    void* arg;            // 函数参数
    int done;             // 是否完成
    struct poly_async_ctx* next;  // 链表指针
} poly_async_ctx;

// 基础协程API
poly_async_ctx* poly_go(poly_async_fn fn, void* arg);  // 创建协程
void poly_yield(void);                                 // 让出执行权
void poly_run(void);                                   // 运行调度器

// 内存管理API
void* poly_alloc(size_t size);                        // 在当前协程栈上分配内存
void poly_reset(void);                                // 重置当前协程的内存使用

// 获取当前协程上下文
poly_async_ctx* poly_current(void);

/**
// 一个简单的消息处理结构
struct message {
    int type;
    char data[128];
};

void process_messages(void* arg) {
    // 在协程栈上分配消息缓冲区
    struct message* msg = poly_alloc(sizeof(struct message));
    
    // 在协程栈上分配结果数组
    int* results = poly_alloc(sizeof(int) * 10);
    int result_count = 0;
    
    while (1) {
        // 读取消息到我们分配的缓冲区
        if (read_message(msg) <= 0) break;
        
        // 处理消息
        if (msg->type == MSG_DATA) {
            // 将处理结果存入结果数组
            results[result_count++] = process_data(msg->data);
        }
        
        // 如果结果足够多，发送出去
        if (result_count >= 10) {
            send_results(results, result_count);
            result_count = 0;
        }
        
        // 让其他协程运行
        poly_yield();
    }
    
    // 发送剩余的结果
    if (result_count > 0) {
        send_results(results, result_count);
    }
    
    // 不需要手动释放 msg 和 results
    // 协程结束时会自动释放所有通过 poly_alloc 分配的内存
}

int main() {
    // 启动消息处理协程
    poly_go(process_messages, NULL);
    
    // 运行协程调度器
    while (1) {
        poly_run();
    }
}
 */