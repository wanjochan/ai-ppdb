/*
 * infra_async.c - Unified Asynchronous System Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_async.h"

//-----------------------------------------------------------------------------
// Internal Types
//-----------------------------------------------------------------------------

// Event loop structure
struct infra_loop {
    bool running;                  // Loop running flag
    infra_mutex_t* lock;          // Loop lock
    infra_event_t* events;        // Event list
    infra_timer_t* timers;        // Timer list
    void* backend;                // Platform-specific backend (epoll/kqueue/etc)
    infra_stats_t stats;          // Statistics
};

//-----------------------------------------------------------------------------
// Loop Management Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_loop_create(infra_loop_t** loop) {
    if (!loop) {
        return INFRA_ERR_PARAM;
    }

    infra_loop_t* new_loop = malloc(sizeof(infra_loop_t));
    if (!new_loop) {
        return INFRA_ERR_MEMORY;
    }

    memset(new_loop, 0, sizeof(infra_loop_t));
    
    infra_error_t err = infra_mutex_create(&new_loop->lock);
    if (err != INFRA_OK) {
        free(new_loop);
        return err;
    }

    // Initialize platform-specific backend
    err = infra_platform_backend_init(&new_loop->backend);
    if (err != INFRA_OK) {
        infra_mutex_destroy(new_loop->lock);
        free(new_loop);
        return err;
    }

    *loop = new_loop;
    return INFRA_OK;
}

infra_error_t infra_loop_destroy(infra_loop_t* loop) {
    if (!loop) {
        return INFRA_ERR_PARAM;
    }

    if (loop->running) {
        return INFRA_ERR_BUSY;
    }

    // Cleanup all events
    infra_event_t* event = loop->events;
    while (event) {
        infra_event_t* next = event->next;
        infra_event_destroy(event);
        event = next;
    }

    // Cleanup all timers
    infra_timer_t* timer = loop->timers;
    while (timer) {
        infra_timer_t* next = timer->next;
        infra_timer_destroy(timer);
        timer = next;
    }

    // Cleanup platform backend
    infra_platform_backend_cleanup(loop->backend);

    infra_mutex_destroy(loop->lock);
    free(loop);
    return INFRA_OK;
}

infra_error_t infra_loop_run(infra_loop_t* loop) {
    if (!loop) {
        return INFRA_ERR_PARAM;
    }

    infra_mutex_lock(loop->lock);
    if (loop->running) {
        infra_mutex_unlock(loop->lock);
        return INFRA_ERR_BUSY;
    }
    loop->running = true;
    infra_mutex_unlock(loop->lock);

    while (loop->running) {
        // Process timers
        uint64_t now = infra_time_monotonic_ms();
        infra_timer_t* timer = loop->timers;
        while (timer) {
            if (timer->next_expire <= now) {
                if (timer->cb) {
                    timer->cb(timer, timer->arg);
                }
                if (timer->repeat) {
                    timer->next_expire = now + timer->interval;
                } else {
                    infra_timer_stop(timer);
                }
            }
            timer = timer->next;
        }

        // Process events
        infra_platform_backend_poll(loop->backend, 100);  // 100ms timeout
    }

    return INFRA_OK;
}

infra_error_t infra_loop_stop(infra_loop_t* loop) {
    if (!loop) {
        return INFRA_ERR_PARAM;
    }

    infra_mutex_lock(loop->lock);
    loop->running = false;
    infra_mutex_unlock(loop->lock);

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Event Management Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_event_create(infra_loop_t* loop, int fd, 
                                uint32_t events, infra_event_cb cb,
                                void* arg, infra_event_t** event) {
    if (!loop || !cb || !event || fd < 0) {
        return INFRA_ERR_PARAM;
    }

    infra_event_t* new_event = malloc(sizeof(infra_event_t));
    if (!new_event) {
        return INFRA_ERR_MEMORY;
    }

    new_event->fd = fd;
    new_event->events = events;
    new_event->cb = cb;
    new_event->arg = arg;
    new_event->loop = loop;
    new_event->data = NULL;

    infra_error_t err = infra_platform_backend_add(loop->backend, new_event);
    if (err != INFRA_OK) {
        free(new_event);
        return err;
    }

    // Add to event list
    infra_mutex_lock(loop->lock);
    new_event->next = loop->events;
    loop->events = new_event;
    loop->stats.events_total++;
    loop->stats.events_active++;
    infra_mutex_unlock(loop->lock);

    *event = new_event;
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Timer Management Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_timer_create(infra_loop_t* loop, uint64_t interval,
                                bool repeat, infra_timer_cb cb,
                                void* arg, infra_timer_t** timer) {
    if (!loop || !cb || !timer || interval == 0) {
        return INFRA_ERR_PARAM;
    }

    infra_timer_t* new_timer = malloc(sizeof(infra_timer_t));
    if (!new_timer) {
        return INFRA_ERR_MEMORY;
    }

    new_timer->interval = interval;
    new_timer->next_expire = infra_time_monotonic_ms() + interval;
    new_timer->repeat = repeat;
    new_timer->cb = cb;
    new_timer->arg = arg;
    new_timer->loop = loop;
    new_timer->data = NULL;

    // Add to timer list
    infra_mutex_lock(loop->lock);
    new_timer->next = loop->timers;
    loop->timers = new_timer;
    loop->stats.timers_total++;
    loop->stats.timers_active++;
    infra_mutex_unlock(loop->lock);

    *timer = new_timer;
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Async IO Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_async_read(infra_loop_t* loop, int fd,
                              void* buf, size_t len,
                              infra_event_cb cb, void* arg) {
    if (!loop || !buf || !cb || fd < 0) {
        return INFRA_ERR_PARAM;
    }

    // Create read event
    infra_event_t* event;
    return infra_event_create(loop, fd, INFRA_EVENT_READ, cb, arg, &event);
}

infra_error_t infra_async_write(infra_loop_t* loop, int fd,
                               const void* buf, size_t len,
                               infra_event_cb cb, void* arg) {
    if (!loop || !buf || !cb || fd < 0) {
        return INFRA_ERR_PARAM;
    }

    // Create write event
    infra_event_t* event;
    return infra_event_create(loop, fd, INFRA_EVENT_WRITE, cb, arg, &event);
}
