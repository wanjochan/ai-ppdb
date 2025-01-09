/*
 * base_timer.inc.c - Timer Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Timer wheel structure
typedef struct ppdb_timer_wheel_s {
    ppdb_base_timer_t* slots[PPDB_TIMER_WHEEL_SIZE];
    uint32_t current;
} ppdb_timer_wheel_t;

// Timer manager structure
typedef struct ppdb_timer_manager_s {
    ppdb_timer_wheel_t wheels[PPDB_TIMER_WHEEL_COUNT];
    ppdb_base_mutex_t* lock;
    uint64_t current_time;
    uint64_t start_time;
    size_t total_timers;
    size_t active_timers;
    size_t expired_timers;
    size_t overdue_timers;
    uint64_t total_drift;
} ppdb_timer_manager_t;

// Global timer manager
static ppdb_timer_manager_t* g_timer_manager = NULL;

// Initialize timer manager
static ppdb_error_t init_timer_manager(void) {
    if (g_timer_manager) return PPDB_OK;
    
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_timer_manager_t), (void**)&g_timer_manager);
    if (err != PPDB_OK) return err;
    
    memset(g_timer_manager, 0, sizeof(ppdb_timer_manager_t));
    
    // Initialize mutex
    err = ppdb_base_mutex_create(&g_timer_manager->lock);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(g_timer_manager);
        g_timer_manager = NULL;
        return err;
    }
    
    // Get current time
    err = ppdb_base_time_get_microseconds(&g_timer_manager->current_time);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(g_timer_manager->lock);
        ppdb_base_mem_free(g_timer_manager);
        g_timer_manager = NULL;
        return err;
    }
    
    g_timer_manager->start_time = g_timer_manager->current_time;
    
    return PPDB_OK;
}

// Calculate timer slot
static void calc_timer_slot(uint64_t expires, uint32_t* wheel, uint32_t* slot) {
    uint64_t diff = expires - g_timer_manager->current_time;
    uint64_t ticks = diff / 1000; // Convert to milliseconds
    
    if (ticks < PPDB_TIMER_WHEEL_SIZE) {
        *wheel = 0;
        *slot = (g_timer_manager->wheels[0].current + ticks) & PPDB_TIMER_WHEEL_MASK;
    } else if (ticks < 1 << (PPDB_TIMER_WHEEL_BITS * 2)) {
        *wheel = 1;
        *slot = ((ticks >> PPDB_TIMER_WHEEL_BITS) + g_timer_manager->wheels[1].current) & PPDB_TIMER_WHEEL_MASK;
    } else if (ticks < 1 << (PPDB_TIMER_WHEEL_BITS * 3)) {
        *wheel = 2;
        *slot = ((ticks >> (PPDB_TIMER_WHEEL_BITS * 2)) + g_timer_manager->wheels[2].current) & PPDB_TIMER_WHEEL_MASK;
    } else {
        *wheel = 3;
        *slot = ((ticks >> (PPDB_TIMER_WHEEL_BITS * 3)) + g_timer_manager->wheels[3].current) & PPDB_TIMER_WHEEL_MASK;
    }
}

// Add timer to wheel
static ppdb_error_t add_timer_to_wheel(ppdb_base_timer_t* timer) {
    uint32_t wheel, slot;
    calc_timer_slot(timer->next_timeout, &wheel, &slot);
    
    timer->next = g_timer_manager->wheels[wheel].slots[slot];
    g_timer_manager->wheels[wheel].slots[slot] = timer;
    g_timer_manager->active_timers++;
    
    return PPDB_OK;
}

// Cascade timers from higher wheel to lower wheel
static void cascade_timers(uint32_t wheel) {
    ppdb_base_timer_t* curr = g_timer_manager->wheels[wheel].slots[g_timer_manager->wheels[wheel].current];
    g_timer_manager->wheels[wheel].slots[g_timer_manager->wheels[wheel].current] = NULL;
    
    while (curr) {
        ppdb_base_timer_t* next = curr->next;
        add_timer_to_wheel(curr);
        curr = next;
    }
}

// Timer creation
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer, uint64_t interval_ms) {
    if (!timer || interval_ms == 0) return PPDB_ERR_PARAM;
    
    ppdb_error_t err = init_timer_manager();
    if (err != PPDB_OK) return err;
    
    ppdb_base_timer_t* new_timer = NULL;
    err = ppdb_base_mem_malloc(sizeof(ppdb_base_timer_t), (void**)&new_timer);
    if (err != PPDB_OK) return err;
    
    new_timer->interval_ms = interval_ms;
    new_timer->next_timeout = g_timer_manager->current_time + interval_ms * 1000;
    new_timer->callback = NULL;
    new_timer->user_data = NULL;
    new_timer->next = NULL;
    new_timer->repeating = false;
    
    memset(&new_timer->stats, 0, sizeof(ppdb_base_timer_stats_t));
    
    err = ppdb_base_mutex_lock(g_timer_manager->lock);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_timer);
        return err;
    }
    
    err = add_timer_to_wheel(new_timer);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(g_timer_manager->lock);
        ppdb_base_mem_free(new_timer);
        return err;
    }
    
    g_timer_manager->total_timers++;
    
    ppdb_base_mutex_unlock(g_timer_manager->lock);
    
    *timer = new_timer;
    return PPDB_OK;
}

// Timer destruction
ppdb_error_t ppdb_base_timer_destroy(ppdb_base_timer_t* timer) {
    if (!timer) return PPDB_ERR_PARAM;
    
    ppdb_error_t err = ppdb_base_mutex_lock(g_timer_manager->lock);
    if (err != PPDB_OK) return err;
    
    // Remove from wheel
    for (int i = 0; i < PPDB_TIMER_WHEEL_COUNT; i++) {
        ppdb_base_timer_t** curr = &g_timer_manager->wheels[i].slots[g_timer_manager->wheels[i].current];
        while (*curr) {
            if (*curr == timer) {
                *curr = timer->next;
                g_timer_manager->active_timers--;
                break;
            }
            curr = &(*curr)->next;
        }
    }
    
    ppdb_base_mutex_unlock(g_timer_manager->lock);
    
    ppdb_base_mem_free(timer);
    return PPDB_OK;
}

// Timer update
ppdb_error_t ppdb_base_timer_update(void) {
    if (!g_timer_manager) return PPDB_OK;
    
    ppdb_error_t err = ppdb_base_mutex_lock(g_timer_manager->lock);
    if (err != PPDB_OK) return err;
    
    uint64_t now;
    err = ppdb_base_time_get_microseconds(&now);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(g_timer_manager->lock);
        return err;
    }
    
    uint64_t elapsed = (now - g_timer_manager->current_time) / 1000; // Convert to milliseconds
    g_timer_manager->current_time = now;
    
    while (elapsed--) {
        // Process current slot
        ppdb_base_timer_t* curr = g_timer_manager->wheels[0].slots[g_timer_manager->wheels[0].current];
        g_timer_manager->wheels[0].slots[g_timer_manager->wheels[0].current] = NULL;
        
        while (curr) {
            ppdb_base_timer_t* next = curr->next;
            
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
            g_timer_manager->total_drift += (drift > 0) ? drift : -drift;
            
            // Execute callback
            if (curr->callback) {
                curr->callback(curr, curr->user_data);
            }
            
            if (curr->repeating) {
                // Reset for next interval
                curr->next_timeout = now + curr->interval_ms * 1000;
                add_timer_to_wheel(curr);
            } else {
                g_timer_manager->active_timers--;
                g_timer_manager->expired_timers++;
                ppdb_base_mem_free(curr);
            }
            
            curr = next;
        }
        
        // Move to next slot
        g_timer_manager->wheels[0].current = (g_timer_manager->wheels[0].current + 1) & PPDB_TIMER_WHEEL_MASK;
        
        // Cascade timers if needed
        if (g_timer_manager->wheels[0].current == 0) {
            cascade_timers(1);
            if (g_timer_manager->wheels[1].current == 0) {
                cascade_timers(2);
                if (g_timer_manager->wheels[2].current == 0) {
                    cascade_timers(3);
                }
            }
        }
    }
    
    ppdb_base_mutex_unlock(g_timer_manager->lock);
    return PPDB_OK;
}

// Get timer statistics
ppdb_error_t ppdb_base_timer_get_stats(ppdb_base_timer_t* timer,
                                     uint64_t* total_ticks,
                                     uint64_t* min_elapsed,
                                     uint64_t* max_elapsed,
                                     uint64_t* avg_elapsed,
                                     uint64_t* last_elapsed,
                                     uint64_t* drift) {
    if (!timer) return PPDB_ERR_PARAM;
    
    if (total_ticks) *total_ticks = timer->stats.total_calls;
    if (min_elapsed) *min_elapsed = timer->stats.min_elapsed;
    if (max_elapsed) *max_elapsed = timer->stats.max_elapsed;
    if (avg_elapsed && timer->stats.total_calls > 0) {
        *avg_elapsed = timer->stats.total_elapsed / timer->stats.total_calls;
    }
    if (last_elapsed) *last_elapsed = timer->stats.last_elapsed;
    if (drift) *drift = timer->stats.drift;
    
    return PPDB_OK;
}

// Get timer manager statistics
void ppdb_base_timer_get_manager_stats(uint64_t* total_timers,
                                    uint64_t* active_timers,
                                    uint64_t* expired_timers,
                                    uint64_t* overdue_timers,
                                    uint64_t* total_drift) {
    if (!g_timer_manager) return;
    
    if (total_timers) *total_timers = g_timer_manager->total_timers;
    if (active_timers) *active_timers = g_timer_manager->active_timers;
    if (expired_timers) *expired_timers = g_timer_manager->expired_timers;
    if (overdue_timers) *overdue_timers = g_timer_manager->overdue_timers;
    if (total_drift) *total_drift = g_timer_manager->total_drift;
} 