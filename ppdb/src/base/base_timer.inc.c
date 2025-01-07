/*
 * base_timer.inc.c - Timer Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Create timer
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer) {
    if (!timer) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_timer_t* new_timer = (ppdb_base_timer_t*)malloc(sizeof(ppdb_base_timer_t));
    if (!new_timer) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_timer, 0, sizeof(ppdb_base_timer_t));
    *timer = new_timer;
    return PPDB_OK;
}

// Destroy timer
void ppdb_base_timer_destroy(ppdb_base_timer_t* timer) {
    if (!timer) return;
    free(timer);
}

// Start timer
ppdb_error_t ppdb_base_timer_start(ppdb_base_timer_t* timer, uint64_t timeout_ms, bool repeat,
                                  ppdb_base_timer_callback_t callback, void* user_data) {
    if (!timer || !callback || timeout_ms == 0) {
        return PPDB_BASE_ERR_PARAM;
    }

    timer->timeout_us = timeout_ms * 1000;
    timer->next_timeout = ppdb_base_get_time_us() + timer->timeout_us;
    timer->repeat = repeat;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->stats.active_timers++;
    if (timer->stats.active_timers > timer->stats.peak_timers) {
        timer->stats.peak_timers = timer->stats.active_timers;
    }

    return PPDB_OK;
}

// Stop timer
ppdb_error_t ppdb_base_timer_stop(ppdb_base_timer_t* timer) {
    if (!timer) {
        return PPDB_BASE_ERR_PARAM;
    }

    timer->callback = NULL;
    timer->user_data = NULL;
    timer->repeat = false;
    if (timer->stats.active_timers > 0) {
        timer->stats.active_timers--;
    }
    timer->stats.total_cancels++;

    return PPDB_OK;
}

// Reset timer
ppdb_error_t ppdb_base_timer_reset(ppdb_base_timer_t* timer) {
    if (!timer || !timer->callback) {
        return PPDB_BASE_ERR_PARAM;
    }

    timer->next_timeout = ppdb_base_get_time_us() + timer->timeout_us;
    timer->stats.total_resets++;

    return PPDB_OK;
}

// Get timer statistics
void ppdb_base_timer_get_stats(ppdb_base_timer_t* timer, ppdb_base_timer_stats_t* stats) {
    if (!timer || !stats) return;
    memcpy(stats, &timer->stats, sizeof(ppdb_base_timer_stats_t));
}

// Check if timer is active
bool ppdb_base_timer_is_active(ppdb_base_timer_t* timer) {
    return timer && timer->callback != NULL;
}

// Get remaining time
uint64_t ppdb_base_timer_get_remaining(ppdb_base_timer_t* timer) {
    if (!timer || !timer->callback) {
        return 0;
    }

    uint64_t now = ppdb_base_get_time_us();
    if (now >= timer->next_timeout) {
        return 0;
    }

    return (timer->next_timeout - now) / 1000; // Convert to milliseconds
}

// Process timer
void ppdb_base_timer_process(ppdb_base_timer_t* timer) {
    if (!timer || !timer->callback) {
        return;
    }

    uint64_t now = ppdb_base_get_time_us();
    if (now >= timer->next_timeout) {
        timer->callback(timer, timer->user_data);
        timer->stats.total_timeouts++;

        if (timer->repeat) {
            timer->next_timeout = now + timer->timeout_us;
        } else {
            ppdb_base_timer_stop(timer);
        }
    }
}

// Set timer interval
ppdb_error_t ppdb_base_timer_set_interval(ppdb_base_timer_t* timer, uint64_t timeout_ms) {
    if (!timer || timeout_ms == 0) {
        return PPDB_BASE_ERR_PARAM;
    }

    timer->timeout_us = timeout_ms * 1000;
    if (timer->callback) {
        timer->next_timeout = ppdb_base_get_time_us() + timer->timeout_us;
    }

    return PPDB_OK;
}

// Clear timer statistics
void ppdb_base_timer_clear_stats(ppdb_base_timer_t* timer) {
    if (!timer) return;
    memset(&timer->stats, 0, sizeof(ppdb_base_timer_stats_t));
}