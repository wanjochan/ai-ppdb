//-----------------------------------------------------------------------------
// Performance Monitoring Implementation
//-----------------------------------------------------------------------------

struct ppdb_engine_perf_counter {
    atomic_size_t value;
    atomic_size_t min;
    atomic_size_t max;
    atomic_size_t sum;
    atomic_size_t count;
    char name[64];
};

struct ppdb_engine_perf_timer {
    uint64_t start;
    ppdb_engine_perf_counter_t* counter;
};

static ppdb_engine_perf_counter_t* g_perf_counters = NULL;
static size_t g_perf_counter_count = 0;
static ppdb_engine_mutex_t* g_perf_mutex = NULL;

ppdb_error_t ppdb_engine_perf_init(void) {
    if (g_perf_mutex) return PPDB_OK;  // Already initialized

    return ppdb_engine_mutex_create(&g_perf_mutex);
}

ppdb_error_t ppdb_engine_perf_cleanup(void) {
    if (!g_perf_mutex) return PPDB_OK;  // Not initialized

    if (g_perf_counters) {
        ppdb_engine_free(g_perf_counters);
        g_perf_counters = NULL;
    }
    g_perf_counter_count = 0;

    ppdb_engine_mutex_destroy(g_perf_mutex);
    g_perf_mutex = NULL;

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_perf_counter_create(const char* name, 
                                            ppdb_engine_perf_counter_t** counter) {
    if (!name || !counter) return PPDB_ERR_NULL_POINTER;
    if (strlen(name) >= 64) return PPDB_ERR_INVALID_ARGUMENT;

    ppdb_engine_mutex_lock(g_perf_mutex);

    // Reallocate counter array
    size_t new_size = g_perf_counter_count + 1;
    ppdb_engine_perf_counter_t* new_counters = ppdb_engine_realloc(g_perf_counters, 
        new_size * sizeof(ppdb_engine_perf_counter_t));
    
    if (!new_counters) {
        ppdb_engine_mutex_unlock(g_perf_mutex);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    g_perf_counters = new_counters;
    *counter = &g_perf_counters[g_perf_counter_count++];

    // Initialize counter
    memset(*counter, 0, sizeof(ppdb_engine_perf_counter_t));
    strncpy((*counter)->name, name, 63);
    atomic_init(&(*counter)->value, 0);
    atomic_init(&(*counter)->min, SIZE_MAX);
    atomic_init(&(*counter)->max, 0);
    atomic_init(&(*counter)->sum, 0);
    atomic_init(&(*counter)->count, 0);

    ppdb_engine_mutex_unlock(g_perf_mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_perf_counter_increment(ppdb_engine_perf_counter_t* counter) {
    if (!counter) return PPDB_ERR_NULL_POINTER;

    atomic_fetch_add(&counter->value, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_perf_counter_add(ppdb_engine_perf_counter_t* counter, 
                                         size_t value) {
    if (!counter) return PPDB_ERR_NULL_POINTER;

    atomic_fetch_add(&counter->value, value);
    
    // Update statistics
    size_t old_min;
    do {
        old_min = atomic_load(&counter->min);
        if (value >= old_min) break;
    } while (!atomic_compare_exchange_weak(&counter->min, &old_min, value));

    size_t old_max;
    do {
        old_max = atomic_load(&counter->max);
        if (value <= old_max) break;
    } while (!atomic_compare_exchange_weak(&counter->max, &old_max, value));

    atomic_fetch_add(&counter->sum, value);
    atomic_fetch_add(&counter->count, 1);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_perf_counter_get(ppdb_engine_perf_counter_t* counter,
                                         size_t* value) {
    if (!counter || !value) return PPDB_ERR_NULL_POINTER;

    *value = atomic_load(&counter->value);
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_perf_counter_stats(ppdb_engine_perf_counter_t* counter,
                                           size_t* min,
                                           size_t* max,
                                           double* avg) {
    if (!counter || !min || !max || !avg) return PPDB_ERR_NULL_POINTER;

    *min = atomic_load(&counter->min);
    *max = atomic_load(&counter->max);
    
    size_t sum = atomic_load(&counter->sum);
    size_t count = atomic_load(&counter->count);
    *avg = count > 0 ? (double)sum / count : 0.0;

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_perf_timer_start(ppdb_engine_perf_counter_t* counter,
                                         ppdb_engine_perf_timer_t** timer) {
    if (!counter || !timer) return PPDB_ERR_NULL_POINTER;

    *timer = ppdb_engine_alloc(sizeof(ppdb_engine_perf_timer_t));
    if (!*timer) return PPDB_ERR_OUT_OF_MEMORY;

    (*timer)->counter = counter;
    (*timer)->start = ppdb_engine_get_time_us();

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_perf_timer_stop(ppdb_engine_perf_timer_t* timer) {
    if (!timer) return PPDB_ERR_NULL_POINTER;

    uint64_t end = ppdb_engine_get_time_us();
    uint64_t elapsed = end - timer->start;

    ppdb_error_t err = ppdb_engine_perf_counter_add(timer->counter, elapsed);
    ppdb_engine_free(timer);

    return err;
}

ppdb_error_t ppdb_engine_perf_report(void) {
    if (!g_perf_mutex) return PPDB_ERR_NOT_INITIALIZED;

    ppdb_engine_mutex_lock(g_perf_mutex);

    printf("\nPerformance Report:\n");
    printf("==================\n");

    for (size_t i = 0; i < g_perf_counter_count; i++) {
        ppdb_engine_perf_counter_t* counter = &g_perf_counters[i];
        size_t value = atomic_load(&counter->value);
        size_t min, max;
        double avg;
        ppdb_engine_perf_counter_stats(counter, &min, &max, &avg);

        printf("Counter: %s\n", counter->name);
        printf("  Value: %zu\n", value);
        printf("  Min: %zu\n", min);
        printf("  Max: %zu\n", max);
        printf("  Avg: %.2f\n", avg);
        printf("\n");
    }

    ppdb_engine_mutex_unlock(g_perf_mutex);
    return PPDB_OK;
}
