/*
 * base_timer.inc.c - Timer Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Create timer
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer, uint64_t interval_ms, bool repeat, ppdb_base_timer_callback_t callback, void* user_data) {
    if (!timer || !callback || interval_ms == 0) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_timer_t* new_timer = (ppdb_base_timer_t*)malloc(sizeof(ppdb_base_timer_t));
    if (!new_timer) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_timer, 0, sizeof(ppdb_base_timer_t));
    new_timer->callback = callback;
    new_timer->user_data = user_data;
    new_timer->interval_ms = interval_ms;
    new_timer->repeat = repeat;
    new_timer->active = false;

    *timer = new_timer;
    return PPDB_OK;
}

// Destroy timer
ppdb_error_t ppdb_base_timer_destroy(ppdb_base_timer_t* timer) {
    if (!timer) return PPDB_BASE_ERR_PARAM;
    free(timer);
    return PPDB_OK;
}

// Start timer
ppdb_error_t ppdb_base_timer_start(ppdb_base_timer_t* timer) {
    if (!timer || !timer->callback) {
        return PPDB_BASE_ERR_PARAM;
    }

    timer->active = true;
    timer->stats.total_ticks = 0;
    timer->stats.total_elapsed = 0;
    timer->stats.min_elapsed = UINT64_MAX;
    timer->stats.max_elapsed = 0;
    timer->stats.avg_elapsed = 0;

    return PPDB_OK;
}

// Stop timer
ppdb_error_t ppdb_base_timer_stop(ppdb_base_timer_t* timer) {
    if (!timer) {
        return PPDB_BASE_ERR_PARAM;
    }

    timer->active = false;
    return PPDB_OK;
}

// Reset timer
ppdb_error_t ppdb_base_timer_reset(ppdb_base_timer_t* timer) {
    if (!timer || !timer->callback) {
        return PPDB_BASE_ERR_PARAM;
    }

    timer->next_timeout = ppdb_base_get_time_us() + (timer->interval_ms * 1000);
    timer->stats.total_ticks++;

    return PPDB_OK;
}

// Get timer statistics
ppdb_error_t ppdb_base_timer_get_stats(ppdb_base_timer_t* timer, ppdb_base_timer_stats_t* stats) {
    if (!timer || !stats) return PPDB_BASE_ERR_PARAM;
    memcpy(stats, &timer->stats, sizeof(ppdb_base_timer_stats_t));
    return PPDB_OK;
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
ppdb_error_t ppdb_base_timer_process(ppdb_base_timer_t* timer) {
    if (!timer || !timer->callback || !timer->active) {
        return PPDB_BASE_ERR_PARAM;
    }

    uint64_t now = ppdb_base_get_time_us();
    if (now >= timer->next_timeout) {
        timer->callback(timer, timer->user_data);
        timer->stats.total_ticks++;

        if (timer->repeat) {
            timer->next_timeout = now + (timer->interval_ms * 1000);
        } else {
            ppdb_base_timer_stop(timer);
        }
    }
    return PPDB_OK;
}

// Set timer interval
ppdb_error_t ppdb_base_timer_set_interval(ppdb_base_timer_t* timer, uint64_t interval_ms) {
    if (!timer || interval_ms == 0) {
        return PPDB_BASE_ERR_PARAM;
    }

    timer->interval_ms = interval_ms;
    if (timer->active) {
        timer->next_timeout = ppdb_base_get_time_us() + (timer->interval_ms * 1000);
    }

    return PPDB_OK;
}

// Clear timer statistics
ppdb_error_t ppdb_base_timer_clear_stats(ppdb_base_timer_t* timer) {
    if (!timer) return PPDB_BASE_ERR_PARAM;
    memset(&timer->stats, 0, sizeof(ppdb_base_timer_stats_t));
    return PPDB_OK;
}