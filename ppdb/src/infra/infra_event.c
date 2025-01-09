#include "cosmopolitan.h"
#include "internal/infra/infra.h"

// Create event loop
int infra_event_loop_create(infra_event_loop_t** loop) {
    if (!loop) return -1;
    
    infra_event_loop_t* new_loop = (infra_event_loop_t*)malloc(sizeof(infra_event_loop_t));
    if (!new_loop) return -1;
    
    memset(new_loop, 0, sizeof(infra_event_loop_t));
    new_loop->running = false;
    new_loop->events = NULL;
    new_loop->event_count = 0;
    new_loop->active_timers = 0;
    new_loop->total_timers = 0;
    
    // Initialize epoll
    new_loop->epoll_fd = epoll_create1(0);
    if (new_loop->epoll_fd < 0) {
        free(new_loop);
        return -1;
    }
    
    // Initialize timer wheels
    for (int i = 0; i < INFRA_TIMER_WHEEL_COUNT; i++) {
        new_loop->wheels[i].current = 0;
        memset(new_loop->wheels[i].slots, 0, sizeof(infra_timer_t*) * INFRA_TIMER_WHEEL_SIZE);
    }
    
    // Get current time
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        close(new_loop->epoll_fd);
        free(new_loop);
        return -1;
    }
    
    new_loop->current_time = ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
    new_loop->start_time = new_loop->current_time;
    
    *loop = new_loop;
    return 0;
}

// Destroy event loop
int infra_event_loop_destroy(infra_event_loop_t* loop) {
    if (!loop) return -1;
    
    // Stop the loop
    loop->running = false;
    
    // Free all events
    infra_event_t* event = loop->events;
    while (event) {
        infra_event_t* next = event->next;
        free(event);
        event = next;
    }
    
    // Free all timers
    for (int i = 0; i < INFRA_TIMER_WHEEL_COUNT; i++) {
        for (int j = 0; j < INFRA_TIMER_WHEEL_SIZE; j++) {
            infra_timer_t* timer = loop->wheels[i].slots[j];
            while (timer) {
                infra_timer_t* next = timer->next;
                free(timer);
                timer = next;
            }
        }
    }
    
    // Close epoll fd
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
    
    free(loop);
    return 0;
}

// Add event to loop
int infra_event_add(infra_event_loop_t* loop, infra_event_t* event) {
    if (!loop || !event) return -1;
    
    struct epoll_event ev;
    ev.events = EPOLLET; // Edge triggered
    
    if (event->events & INFRA_EVENT_READ) {
        ev.events |= EPOLLIN;
    }
    if (event->events & INFRA_EVENT_WRITE) {
        ev.events |= EPOLLOUT;
    }
    
    ev.data.ptr = event;
    
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, event->fd, &ev) < 0) {
        return -1;
    }
    
    event->next = loop->events;
    loop->events = event;
    loop->event_count++;
    
    return 0;
}

// Remove event from loop
int infra_event_remove(infra_event_loop_t* loop, infra_event_t* event) {
    if (!loop || !event) return -1;
    
    // Remove from epoll
    struct epoll_event ev;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, event->fd, &ev) < 0) {
        return -1;
    }
    
    // Remove from event list
    infra_event_t** curr = &loop->events;
    while (*curr) {
        if (*curr == event) {
            *curr = event->next;
            loop->event_count--;
            return 0;
        }
        curr = &(*curr)->next;
    }
    
    return -1;
}

// Modify event in loop
int infra_event_modify(infra_event_loop_t* loop, infra_event_t* event) {
    if (!loop || !event) return -1;
    
    struct epoll_event ev;
    ev.events = EPOLLET;
    
    if (event->events & INFRA_EVENT_READ) {
        ev.events |= EPOLLIN;
    }
    if (event->events & INFRA_EVENT_WRITE) {
        ev.events |= EPOLLOUT;
    }
    
    ev.data.ptr = event;
    
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, event->fd, &ev) < 0) {
        return -1;
    }
    
    return 0;
}

// Calculate timer slot
static void calc_timer_slot(infra_event_loop_t* loop, uint64_t expires, uint32_t* wheel, uint32_t* slot) {
    uint64_t diff = expires - loop->current_time;
    uint64_t ticks = diff / 1000; // Convert to milliseconds
    
    if (ticks < INFRA_TIMER_WHEEL_SIZE) {
        *wheel = 0;
        *slot = (loop->wheels[0].current + ticks) & INFRA_TIMER_WHEEL_MASK;
    } else if (ticks < 1 << (INFRA_TIMER_WHEEL_BITS * 2)) {
        *wheel = 1;
        *slot = ((ticks >> INFRA_TIMER_WHEEL_BITS) + loop->wheels[1].current) & INFRA_TIMER_WHEEL_MASK;
    } else if (ticks < 1 << (INFRA_TIMER_WHEEL_BITS * 3)) {
        *wheel = 2;
        *slot = ((ticks >> (INFRA_TIMER_WHEEL_BITS * 2)) + loop->wheels[2].current) & INFRA_TIMER_WHEEL_MASK;
    } else {
        *wheel = 3;
        *slot = ((ticks >> (INFRA_TIMER_WHEEL_BITS * 3)) + loop->wheels[3].current) & INFRA_TIMER_WHEEL_MASK;
    }
}

// Add timer to wheel
static int add_timer_to_wheel(infra_event_loop_t* loop, infra_timer_t* timer) {
    uint32_t wheel, slot;
    calc_timer_slot(loop, timer->next_timeout, &wheel, &slot);
    
    timer->next = loop->wheels[wheel].slots[slot];
    loop->wheels[wheel].slots[slot] = timer;
    loop->active_timers++;
    
    return 0;
}

// Cascade timers from higher wheel to lower wheel
static void cascade_timers(infra_event_loop_t* loop, uint32_t wheel) {
    infra_timer_t* curr = loop->wheels[wheel].slots[loop->wheels[wheel].current];
    loop->wheels[wheel].slots[loop->wheels[wheel].current] = NULL;
    
    while (curr) {
        infra_timer_t* next = curr->next;
        add_timer_to_wheel(loop, curr);
        curr = next;
    }
}

// Run event loop
int infra_event_loop_run(infra_event_loop_t* loop, int timeout_ms) {
    if (!loop) return -1;
    
    struct epoll_event events[64];
    struct timespec ts;
    
    loop->running = true;
    
    while (loop->running) {
        // Get current time
        if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
            return -1;
        }
        
        uint64_t now = ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
        uint64_t elapsed = (now - loop->current_time) / 1000; // Convert to milliseconds
        loop->current_time = now;
        
        // Process timers
        while (elapsed--) {
            // Process current slot
            infra_timer_t* curr = loop->wheels[0].slots[loop->wheels[0].current];
            loop->wheels[0].slots[loop->wheels[0].current] = NULL;
            
            while (curr) {
                infra_timer_t* next = curr->next;
                
                // Update statistics
                curr->stats.total_calls++;
                uint64_t actual_elapsed = (now - curr->next_timeout) / 1000;
                curr->stats.last_elapsed = actual_elapsed;
                curr->stats.total_elapsed += actual_elapsed;
                if (actual_elapsed > curr->stats.max_elapsed) curr->stats.max_elapsed = actual_elapsed;
                if (actual_elapsed < curr->stats.min_elapsed || curr->stats.min_elapsed == 0) {
                    curr->stats.min_elapsed = actual_elapsed;
                }
                
                // Calculate drift
                int64_t drift = actual_elapsed - curr->interval_ms;
                curr->stats.drift += (drift > 0) ? drift : -drift;
                loop->total_drift += (drift > 0) ? drift : -drift;
                
                // Execute callback
                if (curr->callback) {
                    curr->callback(curr, curr->user_data);
                }
                
                if (curr->repeating) {
                    // Reset for next interval
                    curr->next_timeout = now + curr->interval_ms * 1000;
                    add_timer_to_wheel(loop, curr);
                } else {
                    loop->active_timers--;
                    loop->expired_timers++;
                    free(curr);
                }
                
                curr = next;
            }
            
            // Move to next slot
            loop->wheels[0].current = (loop->wheels[0].current + 1) & INFRA_TIMER_WHEEL_MASK;
            
            // Cascade timers if needed
            if (loop->wheels[0].current == 0) {
                cascade_timers(loop, 1);
                if (loop->wheels[1].current == 0) {
                    cascade_timers(loop, 2);
                    if (loop->wheels[2].current == 0) {
                        cascade_timers(loop, 3);
                    }
                }
            }
        }
        
        // Wait for events
        int nfds = epoll_wait(loop->epoll_fd, events, 64, timeout_ms);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        
        // Process events
        for (int i = 0; i < nfds; i++) {
            infra_event_t* event = events[i].data.ptr;
            uint32_t ev = 0;
            
            if (events[i].events & EPOLLIN) {
                ev |= INFRA_EVENT_READ;
            }
            if (events[i].events & EPOLLOUT) {
                ev |= INFRA_EVENT_WRITE;
            }
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                ev |= INFRA_EVENT_ERROR;
            }
            
            if (event->handler) {
                event->handler(event, ev);
            }
        }
    }
    
    return 0;
}
