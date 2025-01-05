//-----------------------------------------------------------------------------
// Timer Implementation
//-----------------------------------------------------------------------------

struct ppdb_core_timer {
    ppdb_core_async_loop_t* loop;
    int timer_fd;
    ppdb_core_async_handle_t* handle;
    ppdb_core_async_cb callback;
    void* user_data;
    bool repeat;
    uint64_t interval_ms;
};

ppdb_error_t ppdb_core_timer_create(ppdb_core_async_loop_t* loop,
                                   ppdb_core_timer_t** timer) {
    if (!loop || !timer) return PPDB_ERR_NULL_POINTER;

    *timer = ppdb_core_alloc(sizeof(ppdb_core_timer_t));
    if (!*timer) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*timer, 0, sizeof(ppdb_core_timer_t));
    (*timer)->loop = loop;

    // 创建定时器文件描述符
    (*timer)->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if ((*timer)->timer_fd < 0) {
        ppdb_core_free(*timer);
        *timer = NULL;
        return PPDB_ERR_INTERNAL;
    }

    // 创建异步句柄
    ppdb_error_t err = ppdb_core_async_handle_create(loop, (*timer)->timer_fd, &(*timer)->handle);
    if (err != PPDB_OK) {
        close((*timer)->timer_fd);
        ppdb_core_free(*timer);
        *timer = NULL;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_timer_destroy(ppdb_core_timer_t* timer) {
    if (!timer) return PPDB_ERR_NULL_POINTER;

    if (timer->handle) {
        ppdb_core_async_handle_destroy(timer->handle);
    }

    if (timer->timer_fd >= 0) {
        close(timer->timer_fd);
    }

    ppdb_core_free(timer);
    return PPDB_OK;
}

static void timer_callback(ppdb_core_async_handle_t* handle, int status) {
    ppdb_core_timer_t* timer = handle->data;
    if (!timer || !timer->callback) return;

    uint64_t expirations;
    if (read(timer->timer_fd, &expirations, sizeof(expirations)) != sizeof(expirations)) {
        return;
    }

    // 调用用户回调
    timer->callback(handle, status);

    // 如果不是重复定时器，停止它
    if (!timer->repeat) {
        struct itimerspec its = {0};
        timerfd_settime(timer->timer_fd, 0, &its, NULL);
    }
}

ppdb_error_t ppdb_core_timer_start(ppdb_core_timer_t* timer,
                                  uint64_t timeout_ms,
                                  bool repeat,
                                  ppdb_core_async_cb cb,
                                  void* user_data) {
    if (!timer || !cb) return PPDB_ERR_NULL_POINTER;

    timer->callback = cb;
    timer->user_data = user_data;
    timer->repeat = repeat;
    timer->interval_ms = timeout_ms;
    timer->handle->data = timer;

    // 设置定时器
    struct itimerspec its = {0};
    its.it_value.tv_sec = timeout_ms / 1000;
    its.it_value.tv_nsec = (timeout_ms % 1000) * 1000000;
    if (repeat) {
        its.it_interval = its.it_value;
    }

    if (timerfd_settime(timer->timer_fd, 0, &its, NULL) < 0) {
        return PPDB_ERR_INTERNAL;
    }

    // 开始监听定时器事件
    return ppdb_core_async_read(timer->handle, NULL, 0, timer_callback);
}

ppdb_error_t ppdb_core_timer_stop(ppdb_core_timer_t* timer) {
    if (!timer) return PPDB_ERR_NULL_POINTER;

    struct itimerspec its = {0};
    if (timerfd_settime(timer->timer_fd, 0, &its, NULL) < 0) {
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_timer_reset(ppdb_core_timer_t* timer) {
    if (!timer) return PPDB_ERR_NULL_POINTER;

    struct itimerspec its = {0};
    its.it_value.tv_sec = timer->interval_ms / 1000;
    its.it_value.tv_nsec = (timer->interval_ms % 1000) * 1000000;
    if (timer->repeat) {
        its.it_interval = its.it_value;
    }

    if (timerfd_settime(timer->timer_fd, 0, &its, NULL) < 0) {
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
}
