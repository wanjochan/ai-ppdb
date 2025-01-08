#include <cosmopolitan.h>
#include "internal/base.h"
#include "test_common.h"

// Test configuration
#define NUM_THREADS 4
#define OPS_PER_THREAD 100000
#define VALUE_SIZE 8
#define MAX_RETRIES 1000  // Maximum retries for lock acquisition

// Forward declarations
struct test_context_s;

// Thread arguments
typedef struct thread_args_s {
    struct test_context_s* ctx;
    int thread_id;
    uint64_t total_time_us;
    uint64_t contention_count;
    uint64_t ops_completed;
    uint64_t retry_count;
} thread_args_t;

// Shared data structure
typedef struct test_context_s {
    ppdb_base_t* base;
    ppdb_base_mutex_t* mutex;
    ppdb_base_spinlock_t* spinlock;
    _Atomic(uint64_t) counter;
    uint64_t shared_buffer[1];
    thread_args_t thread_args[NUM_THREADS];
    ppdb_base_thread_t* threads[NUM_THREADS];
    _Atomic(bool) should_stop;
} test_context_t;

// Mutex thread function
static void __attribute__((used)) mutex_thread_func(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    test_context_t* ctx = args->ctx;
    uint64_t start_time, end_time;
    args->ops_completed = 0;
    args->retry_count = 0;
    
    for (int i = 0; i < OPS_PER_THREAD && !atomic_load(&ctx->should_stop); i++) {
        start_time = ppdb_base_get_time_us();
        
        // Lock with retry
        int retries = 0;
        ppdb_error_t err;
        while (retries < MAX_RETRIES) {
            err = ppdb_base_mutex_lock(ctx->mutex);
            if (err == PPDB_OK) {
                break;
            }
            args->contention_count++;
            args->retry_count++;
            retries++;
            ppdb_base_sleep(1);  // Small backoff (1ms)
        }
        
        if (retries >= MAX_RETRIES) {
            printf("Thread %d: Lock acquisition failed after %d retries (error: %d)\n", 
                   args->thread_id, retries, err);
            fflush(stdout);
            atomic_store(&ctx->should_stop, true);  // Stop all threads
            break;
        }
        
        end_time = ppdb_base_get_time_us();
        args->total_time_us += (end_time - start_time);
        
        // Critical section - use sequential consistency
        uint64_t old_value = atomic_load_explicit(&ctx->counter, memory_order_seq_cst);
        atomic_store_explicit(&ctx->counter, old_value + 1, memory_order_seq_cst);
        ctx->shared_buffer[0]++;
        args->ops_completed++;
        
        // Unlock
        err = ppdb_base_mutex_unlock(ctx->mutex);
        if (err != PPDB_OK) {
            printf("Thread %d: Failed to unlock mutex (error: %d)\n", args->thread_id, err);
            fflush(stdout);
            atomic_store(&ctx->should_stop, true);  // Stop all threads
            break;
        }
        
        // Reduce log frequency
        if (i % 10000 == 0) {
            printf("Thread %d completed %d operations (retries: %lu)\n", 
                   args->thread_id, i, args->retry_count);
            fflush(stdout);
        }
    }
}

// Spinlock thread function
static void __attribute__((used)) spinlock_thread_func(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    test_context_t* ctx = args->ctx;
    uint64_t start_time, end_time;
    args->ops_completed = 0;
    args->retry_count = 0;
    
    for (int i = 0; i < OPS_PER_THREAD && !atomic_load(&ctx->should_stop); i++) {
        start_time = ppdb_base_get_time_us();
        
        // Lock with retry
        int retries = 0;
        ppdb_error_t err;
        while (retries < MAX_RETRIES) {
            err = ppdb_base_spinlock_lock(ctx->spinlock);
            if (err == PPDB_OK) {
                break;
            }
            args->contention_count++;
            args->retry_count++;
            retries++;
            ppdb_base_yield();  // Yield CPU to other threads
        }
        
        if (retries >= MAX_RETRIES) {
            printf("Thread %d: Lock acquisition failed after %d retries (error: %d)\n", 
                   args->thread_id, retries, err);
            fflush(stdout);
            atomic_store(&ctx->should_stop, true);  // Stop all threads
            break;
        }
        
        end_time = ppdb_base_get_time_us();
        args->total_time_us += (end_time - start_time);
        
        // Critical section - use sequential consistency
        uint64_t old_value = atomic_load_explicit(&ctx->counter, memory_order_seq_cst);
        atomic_store_explicit(&ctx->counter, old_value + 1, memory_order_seq_cst);
        ctx->shared_buffer[0]++;
        args->ops_completed++;
        
        // Unlock
        err = ppdb_base_spinlock_unlock(ctx->spinlock);
        if (err != PPDB_OK) {
            printf("Thread %d: Failed to unlock spinlock (error: %d)\n", args->thread_id, err);
            fflush(stdout);
            atomic_store(&ctx->should_stop, true);  // Stop all threads
            break;
        }
        
        // Reduce log frequency
        if (i % 10000 == 0) {
            printf("Thread %d completed %d operations (retries: %lu)\n", 
                   args->thread_id, i, args->retry_count);
            fflush(stdout);
        }
    }
}

// Test mutex performance
static void test_mutex_performance(void) {
    printf("Running mutex performance test...\n");
    fflush(stdout);
    
    // Initialize test context
    test_context_t* ctx = calloc(1, sizeof(test_context_t));
    assert(ctx != NULL);
    
    // Initialize base
    printf("Initializing base...\n");
    fflush(stdout);
    ppdb_error_t err = ppdb_base_init(&ctx->base, &(ppdb_base_config_t){
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = NUM_THREADS,
        .thread_safe = true
    });
    if (err != PPDB_OK) {
        printf("Failed to initialize base (error: %d)\n", err);
        fflush(stdout);
        free(ctx);
        return;
    }
    
    // Initialize mutex and shared memory
    printf("Creating mutex...\n");
    fflush(stdout);
    err = ppdb_base_mutex_create(&ctx->mutex);
    if (err != PPDB_OK) {
        printf("Failed to create mutex (error: %d)\n", err);
        fflush(stdout);
        ppdb_base_destroy(ctx->base);
        free(ctx);
        return;
    }
    
    ppdb_base_mutex_enable_stats(ctx->mutex, true);
    ctx->shared_buffer[0] = 0;
    atomic_store_explicit(&ctx->counter, 0, memory_order_seq_cst);
    atomic_store(&ctx->should_stop, false);
    
    // Start threads
    printf("Starting threads...\n");
    fflush(stdout);
    uint64_t test_start_time = ppdb_base_get_time_us();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        ctx->thread_args[i].ctx = ctx;
        ctx->thread_args[i].thread_id = i;
        ctx->thread_args[i].total_time_us = 0;
        ctx->thread_args[i].contention_count = 0;
        ctx->thread_args[i].ops_completed = 0;
        ctx->thread_args[i].retry_count = 0;
        printf("Creating thread %d...\n", i);
        fflush(stdout);
        
        err = ppdb_base_thread_create(&ctx->threads[i], mutex_thread_func, &ctx->thread_args[i]);
        if (err != PPDB_OK) {
            printf("Failed to create thread %d (error: %d)\n", i, err);
            fflush(stdout);
            atomic_store(&ctx->should_stop, true);  // Stop all threads
            break;
        }
        printf("Thread %d created\n", i);
        fflush(stdout);
    }
    
    // Wait for threads
    printf("Waiting for threads to complete...\n");
    fflush(stdout);
    uint64_t total_time_us = 0;
    uint64_t total_contention = 0;
    uint64_t total_ops_completed = 0;
    uint64_t total_retries = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        if (ctx->threads[i]) {
            printf("Joining thread %d...\n", i);
            fflush(stdout);
            err = ppdb_base_thread_join(ctx->threads[i]);
            if (err != PPDB_OK) {
                printf("Failed to join thread %d (error: %d)\n", i, err);
                fflush(stdout);
            }
            printf("Thread %d joined\n", i);
            fflush(stdout);
            ppdb_base_thread_destroy(ctx->threads[i]);
            total_time_us += ctx->thread_args[i].total_time_us;
            total_contention += ctx->thread_args[i].contention_count;
            total_ops_completed += ctx->thread_args[i].ops_completed;
            total_retries += ctx->thread_args[i].retry_count;
        }
    }
    
    uint64_t test_end_time = ppdb_base_get_time_us();
    uint64_t test_duration = test_end_time - test_start_time;
    
    // Output statistics
    double avg_latency_us = total_ops_completed > 0 ? 
        (double)total_time_us / total_ops_completed : 0.0;
    double ops_per_sec = test_duration > 0 ? 
        (double)total_ops_completed / (test_duration / 1000000.0) : 0.0;
    
    printf("Mutex Performance Results:\n");
    printf("  Total Operations Completed: %zu\n", total_ops_completed);
    printf("  Total Time: %.2f seconds\n", test_duration / 1000000.0);
    printf("  Average Lock Latency: %.2f microseconds\n", avg_latency_us);
    printf("  Operations/Second: %.2f\n", ops_per_sec);
    printf("  Lock Contentions: %zu\n", total_contention);
    printf("  Total Lock Retries: %zu\n", total_retries);
    printf("  Average Retries per Operation: %.2f\n", 
           total_ops_completed > 0 ? (double)total_retries / total_ops_completed : 0.0);
    printf("  Final Counter: %zu\n", atomic_load_explicit(&ctx->counter, memory_order_seq_cst));
    printf("  Shared Buffer: %zu\n", ctx->shared_buffer[0]);
    fflush(stdout);
    
    // Cleanup
    printf("Cleaning up mutex test resources...\n");
    fflush(stdout);
    ppdb_base_mutex_destroy(ctx->mutex);
    ppdb_base_destroy(ctx->base);
    free(ctx);
}

// Test spinlock performance
static void test_spinlock_performance(void) {
    printf("\nRunning spinlock performance test...\n");
    fflush(stdout);
    
    // Initialize test context
    test_context_t* ctx = calloc(1, sizeof(test_context_t));
    assert(ctx != NULL);
    
    // Initialize base
    printf("Initializing base...\n");
    fflush(stdout);
    ppdb_error_t err = ppdb_base_init(&ctx->base, &(ppdb_base_config_t){
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = NUM_THREADS,
        .thread_safe = true
    });
    if (err != PPDB_OK) {
        printf("Failed to initialize base (error: %d)\n", err);
        fflush(stdout);
        free(ctx);
        return;
    }
    
    // Initialize spinlock and shared memory
    printf("Creating spinlock...\n");
    fflush(stdout);
    err = ppdb_base_spinlock_create(&ctx->spinlock);
    if (err != PPDB_OK) {
        printf("Failed to create spinlock (error: %d)\n", err);
        fflush(stdout);
        ppdb_base_destroy(ctx->base);
        free(ctx);
        return;
    }
    
    ppdb_base_spinlock_enable_stats(ctx->spinlock, true);
    ctx->shared_buffer[0] = 0;
    atomic_store_explicit(&ctx->counter, 0, memory_order_seq_cst);
    atomic_store(&ctx->should_stop, false);
    
    // Start threads
    printf("Starting threads...\n");
    fflush(stdout);
    uint64_t test_start_time = ppdb_base_get_time_us();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        ctx->thread_args[i].ctx = ctx;
        ctx->thread_args[i].thread_id = i;
        ctx->thread_args[i].total_time_us = 0;
        ctx->thread_args[i].contention_count = 0;
        ctx->thread_args[i].ops_completed = 0;
        ctx->thread_args[i].retry_count = 0;
        printf("Creating thread %d...\n", i);
        fflush(stdout);
        
        err = ppdb_base_thread_create(&ctx->threads[i], spinlock_thread_func, &ctx->thread_args[i]);
        if (err != PPDB_OK) {
            printf("Failed to create thread %d (error: %d)\n", i, err);
            fflush(stdout);
            atomic_store(&ctx->should_stop, true);  // Stop all threads
            break;
        }
        printf("Thread %d created\n", i);
        fflush(stdout);
    }
    
    // Wait for threads
    printf("Waiting for threads to complete...\n");
    fflush(stdout);
    uint64_t total_time_us = 0;
    uint64_t total_contention = 0;
    uint64_t total_ops_completed = 0;
    uint64_t total_retries = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        if (ctx->threads[i]) {
            printf("Joining thread %d...\n", i);
            fflush(stdout);
            err = ppdb_base_thread_join(ctx->threads[i]);
            if (err != PPDB_OK) {
                printf("Failed to join thread %d (error: %d)\n", i, err);
                fflush(stdout);
            }
            printf("Thread %d joined\n", i);
            fflush(stdout);
            ppdb_base_thread_destroy(ctx->threads[i]);
            total_time_us += ctx->thread_args[i].total_time_us;
            total_contention += ctx->thread_args[i].contention_count;
            total_ops_completed += ctx->thread_args[i].ops_completed;
            total_retries += ctx->thread_args[i].retry_count;
        }
    }
    
    uint64_t test_end_time = ppdb_base_get_time_us();
    uint64_t test_duration = test_end_time - test_start_time;
    
    // Output statistics
    double avg_latency_us = total_ops_completed > 0 ? 
        (double)total_time_us / total_ops_completed : 0.0;
    double ops_per_sec = test_duration > 0 ? 
        (double)total_ops_completed / (test_duration / 1000000.0) : 0.0;
    
    printf("Spinlock Performance Results:\n");
    printf("  Total Operations Completed: %zu\n", total_ops_completed);
    printf("  Total Time: %.2f seconds\n", test_duration / 1000000.0);
    printf("  Average Lock Latency: %.2f microseconds\n", avg_latency_us);
    printf("  Operations/Second: %.2f\n", ops_per_sec);
    printf("  Lock Contentions: %zu\n", total_contention);
    printf("  Total Lock Retries: %zu\n", total_retries);
    printf("  Average Retries per Operation: %.2f\n", 
           total_ops_completed > 0 ? (double)total_retries / total_ops_completed : 0.0);
    printf("  Final Counter: %zu\n", atomic_load_explicit(&ctx->counter, memory_order_seq_cst));
    printf("  Shared Buffer: %zu\n", ctx->shared_buffer[0]);
    fflush(stdout);
    
    // Cleanup
    printf("Cleaning up spinlock test resources...\n");
    fflush(stdout);
    ppdb_base_spinlock_destroy(ctx->spinlock);
    ppdb_base_destroy(ctx->base);
    free(ctx);
}

int main(void) {
    printf("Running Synchronization Performance Tests\n");
    fflush(stdout);
    
    test_mutex_performance();
    test_spinlock_performance();
    
    printf("\nAll performance tests completed\n");
    fflush(stdout);
    return 0;
} 