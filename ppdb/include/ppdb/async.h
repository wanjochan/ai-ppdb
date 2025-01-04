#ifndef PPDB_ASYNC_H
#define PPDB_ASYNC_H
#include <cosmopolitan.h>

typedef struct async_loop_s async_loop_t;
typedef struct async_handle_s async_handle_t;
typedef void (*async_cb)(async_handle_t* handle, int status);

// 核心API - 事件循环
async_loop_t* async_loop_new(void);
void async_loop_free(async_loop_t* loop);
int async_loop_run(async_loop_t* loop, int timeout_ms);

// 核心API - I/O操作
async_handle_t* async_handle_new(async_loop_t* loop, int fd);
void async_handle_free(async_handle_t* handle);
int async_handle_read(async_handle_t* handle, void* buf, size_t len, async_cb cb);

#endif // PPDB_ASYNC_H
