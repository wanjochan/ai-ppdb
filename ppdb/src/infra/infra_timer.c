#include "cosmopolitan.h"
#include "internal/infra/infra.h"

// Create timer
int infra_timer_create(infra_event_loop_t* loop, infra_timer_t** timer, uint64_t interval_ms) {
    if (!loop || !timer || interval_ms == 0) return -1;
    
    infra_timer_t* new_timer = (infra_timer_t*)malloc(sizeof(infra_timer_t));
    if (!new_timer) return -1;
    
    memset(new_timer, 0, sizeof(infra_timer_t));
    new_timer->interval_ms = interval_ms;
    new_timer->next = NULL;
    new_timer->callback = NULL;
    new_timer->user_data = NULL;
    new_timer->repeating = false;
    
    *timer = new_timer;
    return 0;
}

// Destroy timer
int infra_timer_destroy(infra_event_loop_t* loop, infra_timer_t* timer) {
    if (!loop || !timer) return -1;
    
    // Stop timer if running
    infra_timer_stop(loop, timer);
    
    // Free memory
    free(timer);
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

// Start timer
int infra_timer_start(infra_event_loop_t* loop, infra_timer_t* timer, bool repeating) {
    if (!loop || !timer) return -1;
    
    timer->repeating = repeating;
    timer->next_timeout = loop->current_time + timer->interval_ms * 1000;
    
    // Add to wheel
    uint32_t wheel, slot;
    calc_timer_slot(loop, timer->next_timeout, &wheel, &slot);
    
    timer->next = loop->wheels[wheel].slots[slot];
    loop->wheels[wheel].slots[slot] = timer;
    loop->active_timers++;
    
    return 0;
}

// Stop timer
int infra_timer_stop(infra_event_loop_t* loop, infra_timer_t* timer) {
    if (!loop || !timer) return -1;
    
    // Remove from wheel
    for (int i = 0; i < INFRA_TIMER_WHEEL_COUNT; i++) {
        infra_timer_t** curr = &loop->wheels[i].slots[loop->wheels[i].current];
        while (*curr) {
            if (*curr == timer) {
                *curr = timer->next;
                loop->active_timers--;
                return 0;
            }
            curr = &(*curr)->next;
        }
    }
    
    return 0; // Timer not found in any wheel
} 
