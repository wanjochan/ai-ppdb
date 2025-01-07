#include <cosmopolitan.h>
#include "internal/base.h"

// Create a new timer
ppdb_error_t ppdb_base_timer_create(ppdb_base_event_loop_t* loop,
                                   ppdb_base_timer_t** timer) {
    PPDB_CHECK_NULL(loop);
    PPDB_CHECK_NULL(timer);

    ppdb_base_timer_t* t = malloc(sizeof(ppdb_base_timer_t));
    if (!t) return PPDB_BASE_ERR_MEMORY;

    memset(t, 0, sizeof(ppdb_base_timer_t));
    t->loop = loop;

    // Add to timer list
    ppdb_base_mutex_lock(loop->mutex);
    t->next = loop->timers;
    loop->timers = t;
    ppdb_base_mutex_unlock(loop->mutex);

    *timer = t;
    return PPDB_OK;
}

// Start the timer
ppdb_error_t ppdb_base_timer_start(ppdb_base_timer_t* timer,
                                  uint64_t timeout_ms,
                                  bool repeat,
                                  ppdb_base_timer_callback_t callback,
                                  void* user_data) {
    PPDB_CHECK_NULL(timer);
    PPDB_CHECK_NULL(callback);
    PPDB_CHECK_PARAM(timeout_ms > 0);

    ppdb_base_mutex_lock(timer->loop->mutex);

    timer->timeout_us = timeout_ms * 1000;  // Convert to microseconds
    timer->repeat = repeat;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->next_timeout = ppdb_base_get_time_us() + timer->timeout_us;

    // Update statistics
    timer->stats.active_timers++;
    if (timer->stats.active_timers > timer->stats.peak_timers) {
        timer->stats.peak_timers = timer->stats.active_timers;
    }
    timer->stats.total_timeouts++;

    ppdb_base_mutex_unlock(timer->loop->mutex);
    return PPDB_OK;
}

// Stop the timer
ppdb_error_t ppdb_base_timer_stop(ppdb_base_timer_t* timer) {
    PPDB_CHECK_NULL(timer);

    ppdb_base_mutex_lock(timer->loop->mutex);

    if (timer->callback) {
        timer->callback = NULL;
        timer->stats.active_timers--;
        timer->stats.total_cancels++;
    }

    ppdb_base_mutex_unlock(timer->loop->mutex);
    return PPDB_OK;
}

// Reset the timer
ppdb_error_t ppdb_base_timer_reset(ppdb_base_timer_t* timer) {
    PPDB_CHECK_NULL(timer);

    ppdb_base_mutex_lock(timer->loop->mutex);

    if (timer->callback) {
        timer->next_timeout = ppdb_base_get_time_us() + timer->timeout_us;
        timer->stats.total_resets++;
    }

    ppdb_base_mutex_unlock(timer->loop->mutex);
    return PPDB_OK;
}

// Destroy the timer
void ppdb_base_timer_destroy(ppdb_base_timer_t* timer) {
    if (!timer) return;

    ppdb_base_event_loop_t* loop = timer->loop;
    if (!loop) {
        free(timer);
        return;
    }

    // Stop if running
    if (timer->callback) {
        ppdb_base_timer_stop(timer);
    }

    // Remove from timer list
    ppdb_base_mutex_lock(loop->mutex);
    ppdb_base_timer_t** curr = &loop->timers;
    while (*curr) {
        if (*curr == timer) {
            *curr = timer->next;
            break;
        }
        curr = &(*curr)->next;
    }
    ppdb_base_mutex_unlock(loop->mutex);

    free(timer);
}

void ppdb_base_timer_get_stats(ppdb_base_timer_t* timer,
                              ppdb_base_timer_stats_t* stats) {
    if (!timer || !stats) return;

    ppdb_base_mutex_lock(timer->loop->mutex);
    memcpy(stats, &timer->stats, sizeof(ppdb_base_timer_stats_t));
    ppdb_base_mutex_unlock(timer->loop->mutex);
}

void ppdb_base_timer_reset_stats(ppdb_base_timer_t* timer) {
    if (!timer) return;

    ppdb_base_mutex_lock(timer->loop->mutex);
    memset(&timer->stats, 0, sizeof(ppdb_base_timer_stats_t));
    // Preserve active timers count
    timer->stats.active_timers = timer->callback ? 1 : 0;
    ppdb_base_mutex_unlock(timer->loop->mutex);
}