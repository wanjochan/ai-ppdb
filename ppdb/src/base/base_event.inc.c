#include <sys/epoll.h>
#include "internal/base.h"

#define MAX_EVENTS 64

// Platform-specific implementation
typedef struct ppdb_base_event_impl_s {
    int epoll_fd;                  // epoll file descriptor
    struct epoll_event events[MAX_EVENTS];  // Event buffer
} ppdb_base_event_impl_t;

ppdb_error_t ppdb_base_event_loop_create(ppdb_base_event_loop_t** loop) {
    PPDB_CHECK_NULL(loop);

    ppdb_base_event_loop_t* l = malloc(sizeof(ppdb_base_event_loop_t));
    if (!l) return PPDB_BASE_ERR_MEMORY;

    memset(l, 0, sizeof(ppdb_base_event_loop_t));

    // Create mutex
    ppdb_error_t err = ppdb_base_mutex_create(&l->mutex);
    if (err != PPDB_OK) {
        free(l);
        return err;
    }

    // Create platform implementation
    ppdb_base_event_impl_t* impl = malloc(sizeof(ppdb_base_event_impl_t));
    if (!impl) {
        ppdb_base_mutex_destroy(l->mutex);
        free(l);
        return PPDB_BASE_ERR_MEMORY;
    }

    impl->epoll_fd = epoll_create1(0);
    if (impl->epoll_fd < 0) {
        free(impl);
        ppdb_base_mutex_destroy(l->mutex);
        free(l);
        return PPDB_BASE_ERR_IO;
    }

    l->impl = impl;
    *loop = l;
    return PPDB_OK;
}

void ppdb_base_event_loop_destroy(ppdb_base_event_loop_t* loop) {
    if (!loop) return;

    ppdb_base_mutex_lock(loop->mutex);

    // Stop if running
    if (loop->running) {
        ppdb_base_event_loop_stop(loop);
    }

    // Cleanup handlers
    ppdb_base_event_handler_t* handler = loop->handlers;
    while (handler) {
        ppdb_base_event_handler_t* next = handler->next;
        ppdb_base_event_handler_destroy(handler);
        handler = next;
    }

    // Cleanup timers
    ppdb_base_timer_t* timer = loop->timers;
    while (timer) {
        ppdb_base_timer_t* next = timer->next;
        ppdb_base_timer_destroy(timer);
        timer = next;
    }

    // Cleanup platform implementation
    if (loop->impl) {
        ppdb_base_event_impl_t* impl = (ppdb_base_event_impl_t*)loop->impl;
        close(impl->epoll_fd);
        free(impl);
    }

    ppdb_base_mutex_unlock(loop->mutex);
    ppdb_base_mutex_destroy(loop->mutex);
    free(loop);
}

ppdb_error_t ppdb_base_event_loop_run(ppdb_base_event_loop_t* loop, uint64_t timeout_ms) {
    PPDB_CHECK_NULL(loop);
    PPDB_CHECK_NULL(loop->impl);

    ppdb_base_event_impl_t* impl = (ppdb_base_event_impl_t*)loop->impl;
    uint64_t start_time = ppdb_base_get_time_us();

    ppdb_base_mutex_lock(loop->mutex);
    if (loop->running) {
        ppdb_base_mutex_unlock(loop->mutex);
        return PPDB_BASE_ERR_INVALID_STATE;
    }
    loop->running = true;
    ppdb_base_mutex_unlock(loop->mutex);

    while (loop->running) {
        // Wait for events
        int nfds = epoll_wait(impl->epoll_fd, impl->events, MAX_EVENTS, timeout_ms);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            loop->running = false;
            return PPDB_BASE_ERR_IO;
        }

        // Process events
        for (int i = 0; i < nfds; i++) {
            ppdb_base_event_handler_t* handler = 
                (ppdb_base_event_handler_t*)impl->events[i].data.ptr;
            uint32_t events = 0;

            if (impl->events[i].events & EPOLLIN)  events |= PPDB_EVENT_READ;
            if (impl->events[i].events & EPOLLOUT) events |= PPDB_EVENT_WRITE;
            if (impl->events[i].events & EPOLLERR) events |= PPDB_EVENT_ERROR;

            // Update statistics
            ppdb_base_mutex_lock(loop->mutex);
            loop->stats.total_events++;
            if (events & PPDB_EVENT_ERROR) {
                loop->stats.total_errors++;
            }
            ppdb_base_mutex_unlock(loop->mutex);

            // Call handler
            if (handler && handler->callback) {
                handler->callback(handler, events);
            }
        }

        // Process timers
        ppdb_base_mutex_lock(loop->mutex);
        ppdb_base_timer_t* timer = loop->timers;
        uint64_t current_time = ppdb_base_get_time_us();
        while (timer) {
            if (current_time >= timer->next_timeout) {
                timer->callback(timer, timer->user_data);
                loop->stats.total_timeouts++;
                
                if (timer->repeat) {
                    timer->next_timeout = current_time + timer->timeout_us;
                } else {
                    ppdb_base_timer_stop(timer);
                }
            }
            timer = timer->next;
        }
        ppdb_base_mutex_unlock(loop->mutex);

        // Update wait time statistics
        loop->stats.total_wait_time_us += (ppdb_base_get_time_us() - start_time);
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_event_loop_stop(ppdb_base_event_loop_t* loop) {
    PPDB_CHECK_NULL(loop);

    ppdb_base_mutex_lock(loop->mutex);
    loop->running = false;
    ppdb_base_mutex_unlock(loop->mutex);

    return PPDB_OK;
}

ppdb_error_t ppdb_base_event_handler_create(ppdb_base_event_loop_t* loop,
                                          ppdb_base_event_handler_t** handler,
                                          int fd, uint32_t events,
                                          void (*callback)(ppdb_base_event_handler_t*, uint32_t),
                                          void* data) {
    PPDB_CHECK_NULL(loop);
    PPDB_CHECK_NULL(handler);
    PPDB_CHECK_NULL(callback);
    PPDB_CHECK_PARAM(fd >= 0);

    ppdb_base_event_handler_t* h = malloc(sizeof(ppdb_base_event_handler_t));
    if (!h) return PPDB_BASE_ERR_MEMORY;

    h->fd = fd;
    h->events = events;
    h->callback = callback;
    h->data = data;
    h->next = NULL;

    // Add to epoll
    struct epoll_event ev;
    ev.events = 0;
    if (events & PPDB_EVENT_READ)  ev.events |= EPOLLIN;
    if (events & PPDB_EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.ptr = h;

    ppdb_base_event_impl_t* impl = (ppdb_base_event_impl_t*)loop->impl;
    if (epoll_ctl(impl->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        free(h);
        return PPDB_BASE_ERR_IO;
    }

    // Add to handler list
    ppdb_base_mutex_lock(loop->mutex);
    h->next = loop->handlers;
    loop->handlers = h;
    loop->stats.active_handlers++;
    ppdb_base_mutex_unlock(loop->mutex);

    *handler = h;
    return PPDB_OK;
}

void ppdb_base_event_handler_destroy(ppdb_base_event_handler_t* handler) {
    if (!handler) return;

    // Remove from epoll
    ppdb_base_event_impl_t* impl = (ppdb_base_event_impl_t*)handler->loop->impl;
    epoll_ctl(impl->epoll_fd, EPOLL_CTL_DEL, handler->fd, NULL);

    // Remove from handler list
    ppdb_base_mutex_lock(handler->loop->mutex);
    ppdb_base_event_handler_t** curr = &handler->loop->handlers;
    while (*curr) {
        if (*curr == handler) {
            *curr = handler->next;
            handler->loop->stats.active_handlers--;
            break;
        }
        curr = &(*curr)->next;
    }
    ppdb_base_mutex_unlock(handler->loop->mutex);

    free(handler);
}

ppdb_error_t ppdb_base_event_handler_modify(ppdb_base_event_handler_t* handler,
                                          uint32_t events) {
    PPDB_CHECK_NULL(handler);

    struct epoll_event ev;
    ev.events = 0;
    if (events & PPDB_EVENT_READ)  ev.events |= EPOLLIN;
    if (events & PPDB_EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.ptr = handler;

    ppdb_base_event_impl_t* impl = (ppdb_base_event_impl_t*)handler->loop->impl;
    if (epoll_ctl(impl->epoll_fd, EPOLL_CTL_MOD, handler->fd, &ev) < 0) {
        return PPDB_BASE_ERR_IO;
    }

    handler->events = events;
    return PPDB_OK;
}

void ppdb_base_event_get_stats(ppdb_base_event_loop_t* loop,
                              ppdb_base_event_stats_t* stats) {
    if (!loop || !stats) return;

    ppdb_base_mutex_lock(loop->mutex);
    memcpy(stats, &loop->stats, sizeof(ppdb_base_event_stats_t));
    ppdb_base_mutex_unlock(loop->mutex);
}

void ppdb_base_event_reset_stats(ppdb_base_event_loop_t* loop) {
    if (!loop) return;

    ppdb_base_mutex_lock(loop->mutex);
    memset(&loop->stats, 0, sizeof(ppdb_base_event_stats_t));
    // Preserve active handlers count
    loop->stats.active_handlers = 0;
    ppdb_base_event_handler_t* handler = loop->handlers;
    while (handler) {
        loop->stats.active_handlers++;
        handler = handler->next;
    }
    ppdb_base_mutex_unlock(loop->mutex);
} 