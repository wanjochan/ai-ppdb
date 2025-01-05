//-----------------------------------------------------------------------------
// Asynchronous Operations Implementation
//-----------------------------------------------------------------------------

struct ppdb_base_async_loop {
    int epoll_fd;           // epoll 文件描述符
    bool is_running;        // 事件循环是否在运行
    ppdb_base_mutex_t* mutex;  // 保护内部状态
};

struct ppdb_base_async_handle {
    ppdb_base_async_loop_t* loop;  // 所属事件循环
    int fd;                        // 文件描述符
    void* data;                    // 用户数据
    ppdb_base_async_cb callback;   // 回调函数
    struct {
        void* buf;                 // 读写缓冲区
        size_t len;                // 缓冲区长度
        size_t pos;                // 当前位置
    } io;
};

struct ppdb_base_async_future {
    ppdb_base_async_loop_t* loop;  // 所属事件循环
    bool is_ready;                 // 是否就绪
    void* result;                  // 结果数据
    ppdb_base_mutex_t* mutex;      // 保护内部状态
    ppdb_base_cond_t* cond;        // 条件变量
};

// Forward all operations to base layer
ppdb_error_t ppdb_base_async_loop_create(ppdb_base_async_loop_t** loop) {
    return ppdb_base_async_create_loop(loop);
}

ppdb_error_t ppdb_base_async_loop_destroy(ppdb_base_async_loop_t* loop) {
    return ppdb_base_async_destroy_loop(loop);
}

ppdb_error_t ppdb_base_async_loop_run(ppdb_base_async_loop_t* loop, int timeout_ms) {
    return ppdb_base_async_run_loop(loop, timeout_ms);
}

ppdb_error_t ppdb_base_async_handle_create(ppdb_base_async_loop_t* loop,
                                          int fd,
                                          ppdb_base_async_handle_t** handle) {
    return ppdb_base_async_create_handle(loop, fd, handle);
}

ppdb_error_t ppdb_base_async_handle_destroy(ppdb_base_async_handle_t* handle) {
    return ppdb_base_async_destroy_handle(handle);
}

ppdb_error_t ppdb_base_async_read(ppdb_base_async_handle_t* handle,
                                 void* buf,
                                 size_t len,
                                 ppdb_base_async_cb cb) {
    return ppdb_base_async_read_handle(handle, buf, len, cb);
}

ppdb_error_t ppdb_base_async_write(ppdb_base_async_handle_t* handle,
                                  const void* buf,
                                  size_t len,
                                  ppdb_base_async_cb cb) {
    return ppdb_base_async_write_handle(handle, buf, len, cb);
}

ppdb_error_t ppdb_base_async_future_create(ppdb_base_async_loop_t* loop,
                                          ppdb_base_async_future_t** future) {
    return ppdb_base_async_create_future(loop, future);
}

ppdb_error_t ppdb_base_async_future_destroy(ppdb_base_async_future_t* future) {
    return ppdb_base_async_destroy_future(future);
}

ppdb_error_t ppdb_base_async_future_wait(ppdb_base_async_future_t* future) {
    return ppdb_base_async_wait_future(future);
}

ppdb_error_t ppdb_base_async_future_is_ready(ppdb_base_async_future_t* future,
                                            bool* ready) {
    return ppdb_base_async_check_future_ready(future, ready);
}
