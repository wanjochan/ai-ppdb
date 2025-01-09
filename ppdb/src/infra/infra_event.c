#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_io.h"
#include "internal/infra/infra_event.h"

#define MAX_EVENTS 64
#define MAX_HANDLERS 128

struct event_loop {
    int epoll_fd;
    bool running;
    struct event_handler handlers[MAX_HANDLERS];
    int handler_fds[MAX_HANDLERS];
    int handler_count;
    struct infra_list timers;  // 定时器链表
    infra_mutex_t lock;        // 保护handlers和timers
};

static int find_handler_slot(struct event_loop* loop, int fd) {
    for (int i = 0; i < loop->handler_count; i++) {
        if (loop->handler_fds[i] == fd) {
            return i;
        }
    }
    return -1;
}

int infra_event_loop_init(struct infra_event_loop* loop) {
    struct event_loop* el = infra_calloc(1, sizeof(struct event_loop));
    if (!el) {
        infra_set_error(INFRA_ERR_NOMEM, "Failed to allocate event loop");
        return -1;
    }

    el->epoll_fd = epoll_create1(0);
    if (el->epoll_fd < 0) {
        infra_free(el);
        infra_set_error(INFRA_ERR_NETWORK, "Failed to create epoll fd");
        return -1;
    }

    infra_list_init(&el->timers);
    infra_mutex_init(&el->lock);
    el->handler_count = 0;
    loop->impl = el;
    return 0;
}

int event_add_handler(struct infra_event_loop* loop, int fd, event_handler_fn handler, void* arg) {
    struct event_loop* el = loop->impl;
    infra_mutex_lock(&el->lock);

    if (el->handler_count >= MAX_HANDLERS) {
        infra_mutex_unlock(&el->lock);
        infra_set_error(INFRA_ERR_BUSY, "Too many handlers");
        return -1;
    }

    int slot = find_handler_slot(el, fd);
    if (slot >= 0) {
        // Update existing handler
        el->handlers[slot].fn = handler;
        el->handlers[slot].arg = arg;
        infra_mutex_unlock(&el->lock);
        return 0;
    }

    // Add new handler
    slot = el->handler_count++;
    el->handler_fds[slot] = fd;
    el->handlers[slot].fn = handler;
    el->handlers[slot].arg = arg;

    infra_mutex_unlock(&el->lock);
    return 0;
}

int event_del_handler(struct infra_event_loop* loop, int fd) {
    struct event_loop* el = loop->impl;
    infra_mutex_lock(&el->lock);

    int slot = find_handler_slot(el, fd);
    if (slot < 0) {
        infra_mutex_unlock(&el->lock);
        infra_set_error(INFRA_ERR_NOTFOUND, "Handler not found");
        return -1;
    }

    // Remove handler by moving the last one to this slot
    el->handler_count--;
    if (slot < el->handler_count) {
        el->handler_fds[slot] = el->handler_fds[el->handler_count];
        el->handlers[slot] = el->handlers[el->handler_count];
    }

    infra_mutex_unlock(&el->lock);
    return 0;
}

int event_add_io(struct infra_event_loop* loop, int fd, int events, void (*handler)(int fd, void* arg), void* arg) {
    struct event_loop* el = loop->impl;
    struct epoll_event ev;
    ev.events = 0;
    
    if (events & EVENT_READ) {
        ev.events |= EPOLLIN;
    }
    if (events & EVENT_WRITE) {
        ev.events |= EPOLLOUT;
    }
    if (events & EVENT_ERROR) {
        ev.events |= EPOLLERR;
    }
    
    ev.data.fd = fd;

    int ret = event_add_handler(loop, fd, handler, arg);
    if (ret != 0) {
        return ret;
    }

    ret = epoll_ctl(el->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0) {
        event_del_handler(loop, fd);
        infra_set_error(INFRA_ERR_NETWORK, "Failed to add IO event");
        return -1;
    }

    return 0;
}

int event_mod_io(struct infra_event_loop* loop, int fd, int events) {
    struct event_loop* el = loop->impl;
    struct epoll_event ev;
    ev.events = 0;
    
    if (events & EVENT_READ) {
        ev.events |= EPOLLIN;
    }
    if (events & EVENT_WRITE) {
        ev.events |= EPOLLOUT;
    }
    if (events & EVENT_ERROR) {
        ev.events |= EPOLLERR;
    }
    
    ev.data.fd = fd;

    int ret = epoll_ctl(el->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    if (ret != 0) {
        infra_set_error(INFRA_ERR_NETWORK, "Failed to modify IO event");
        return -1;
    }

    return 0;
}

static void process_timers(struct event_loop* el) {
    struct infra_list* pos;
    struct infra_list* n;
    u64 now = time(NULL);

    infra_mutex_lock(&el->lock);
    infra_list_for_each_safe(pos, n, &el->timers) {
        struct infra_timer* timer = container_of(pos, struct infra_timer, list);
        if (timer->deadline <= now) {
            infra_list_del(&timer->list);
            timer->handler(timer->ctx, INFRA_EVENT_ERROR);
        }
    }
    infra_mutex_unlock(&el->lock);
}

int infra_event_loop_run(struct infra_event_loop* loop) {
    struct event_loop* el = loop->impl;
    struct epoll_event events[MAX_EVENTS];
    el->running = true;

    while (el->running) {
        int nfds = epoll_wait(el->epoll_fd, events, MAX_EVENTS, 1000); // 1秒超时用于处理定时器
        
        if (nfds > 0) {
            infra_mutex_lock(&el->lock);
            for (int i = 0; i < nfds; i++) {
                int fd = events[i].data.fd;
                int slot = find_handler_slot(el, fd);
                if (slot >= 0) {
                    struct event_handler* handler = &el->handlers[slot];
                    handler->fn(fd, handler->arg);
                }
            }
            infra_mutex_unlock(&el->lock);
        } else if (nfds == 0) {
            // Timeout, process timers
            process_timers(el);
        } else if (errno != EINTR) {
            infra_set_error(INFRA_ERR_NETWORK, "epoll_wait failed");
            return -1;
        }
    }

    return 0;
}

void infra_event_loop_stop(struct infra_event_loop* loop) {
    struct event_loop* el = loop->impl;
    if (el) {
        el->running = false;
    }
}

void infra_event_loop_destroy(struct infra_event_loop* loop) {
    struct event_loop* el = loop->impl;
    if (el) {
        close(el->epoll_fd);
        infra_mutex_destroy(&el->lock);
        infra_free(el);
        loop->impl = NULL;
    }
}

int infra_timer_init(struct infra_timer* timer, u64 deadline, infra_event_handler handler, void* ctx) {
    if (!timer || !handler) {
        infra_set_error(INFRA_ERR_INVALID, "Invalid timer parameters");
        return -1;
    }

    timer->deadline = deadline;
    timer->handler = handler;
    timer->ctx = ctx;
    infra_list_init(&timer->list);
    return 0;
}

int infra_timer_add(struct infra_event_loop* loop, struct infra_timer* timer) {
    struct event_loop* el = loop->impl;
    if (!el || !timer) {
        infra_set_error(INFRA_ERR_INVALID, "Invalid parameters");
        return -1;
    }

    infra_mutex_lock(&el->lock);
    infra_list_add(&el->timers, &timer->list);
    infra_mutex_unlock(&el->lock);
    return 0;
}

int infra_timer_del(struct infra_event_loop* loop, struct infra_timer* timer) {
    struct event_loop* el = loop->impl;
    if (!el || !timer) {
        infra_set_error(INFRA_ERR_INVALID, "Invalid parameters");
        return -1;
    }

    infra_mutex_lock(&el->lock);
    infra_list_del(&timer->list);
    infra_mutex_unlock(&el->lock);
    return 0;
}
