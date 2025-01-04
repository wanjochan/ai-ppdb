#include "ppdb/async.h"
#include <stdlib.h>

struct async_handle_s {
    async_loop_t* loop;
    int fd;
    void* buf;
    size_t len;
    async_cb cb;
    int active;
};

struct async_loop_s {
    struct pollfd* fds;
    async_handle_t** handles;
    size_t size;
    size_t capacity;
};

async_loop_t* async_loop_new(void) {
    async_loop_t* loop = calloc(1, sizeof(*loop));
    if (!loop) return NULL;

    loop->capacity = 16;
    loop->fds = calloc(loop->capacity, sizeof(*loop->fds));
    loop->handles = calloc(loop->capacity, sizeof(*loop->handles));
    
    if (!loop->fds || !loop->handles) {
        free(loop->fds);
        free(loop->handles);
        free(loop);
        return NULL;
    }
    
    return loop;
}

void async_loop_free(async_loop_t* loop) {
    if (!loop) return;
    free(loop->fds);
    free(loop->handles);
    free(loop);
}

async_handle_t* async_handle_new(async_loop_t* loop, int fd) {
    if (!loop || fd < 0) return NULL;
    
    // 检查容量
    if (loop->size >= loop->capacity) {
        size_t new_cap = loop->capacity * 2;
        void* new_fds = realloc(loop->fds, new_cap * sizeof(*loop->fds));
        void* new_handles = realloc(loop->handles, new_cap * sizeof(*loop->handles));
        
        if (!new_fds || !new_handles) {
            free(new_fds);
            free(new_handles);
            return NULL;
        }
        
        loop->fds = new_fds;
        loop->handles = new_handles;
        loop->capacity = new_cap;
    }
    
    // 创建新handle
    async_handle_t* handle = calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    
    handle->loop = loop;
    handle->fd = fd;
    
    // 添加到loop
    loop->fds[loop->size].fd = fd;
    loop->fds[loop->size].events = 0;
    loop->handles[loop->size] = handle;
    loop->size++;
    
    return handle;
}

void async_handle_free(async_handle_t* handle) {
    if (!handle) return;
    
    async_loop_t* loop = handle->loop;
    for (size_t i = 0; i < loop->size; i++) {
        if (loop->handles[i] == handle) {
            // 移除handle
            loop->size--;
            if (i < loop->size) {
                loop->fds[i] = loop->fds[loop->size];
                loop->handles[i] = loop->handles[loop->size];
            }
            break;
        }
    }
    
    free(handle);
}

int async_handle_read(async_handle_t* handle, void* buf, size_t len, async_cb cb) {
    if (!handle || !buf || !cb) return -1;
    
    handle->buf = buf;
    handle->len = len;
    handle->cb = cb;
    handle->active = 1;
    
    // 设置读事件
    for (size_t i = 0; i < handle->loop->size; i++) {
        if (handle->loop->handles[i] == handle) {
            handle->loop->fds[i].events |= POLLIN;
            break;
        }
    }
    
    return 0;
}

int async_loop_run(async_loop_t* loop, int timeout_ms) {
    if (!loop) return -1;
    
    int n = poll(loop->fds, loop->size, timeout_ms);
    if (n <= 0) return n;
    
    for (size_t i = 0; i < loop->size; i++) {
        async_handle_t* handle = loop->handles[i];
        if (!handle->active) continue;
        
        if (loop->fds[i].revents & POLLIN) {
            ssize_t n = read(handle->fd, handle->buf, handle->len);
            handle->active = 0;
            loop->fds[i].events &= ~POLLIN;
            handle->cb(handle, n);
        }
    }
    
    return n;
}
