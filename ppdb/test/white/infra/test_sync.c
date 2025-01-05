#include <cosmopolitan.h>
#include <ppdb/internal.h>

// Test macros
#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "Assertion failed: %s\n", #cond); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "Assertion failed: %s != %s\n", #a, #b); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            fprintf(stderr, "Assertion failed: %s == %s\n", #a, #b); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "Assertion failed: %s is NULL\n", #ptr); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "Assertion failed: %s is not NULL\n", #ptr); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_OK(err) \
    do { \
        if ((err) != PPDB_OK) { \
            fprintf(stderr, "Assertion failed: %s is not OK\n", #err); \
            exit(1); \
        } \
    } while (0)

#define TEST_SUITE_BEGIN(name) \
    do { \
        printf("Running test suite: %s\n", name); \
    } while (0)

#define TEST_RUN(test) \
    do { \
        printf("  Running test: %s\n", #test); \
        test(); \
        printf("  Test passed: %s\n", #test); \
    } while (0)

#define TEST_SUITE_END() \
    do { \
        printf("Test suite completed\n"); \
    } while (0)

// Test cases
void test_mutex_basic(void) {
    ppdb_engine_mutex_t* mutex = NULL;
    ppdb_error_t err;

    // Create mutex
    err = ppdb_engine_mutex_create(&mutex);
    ASSERT_OK(err);
    ASSERT_NOT_NULL(mutex);

    // Lock mutex
    err = ppdb_engine_mutex_lock(mutex);
    ASSERT_OK(err);

    // Unlock mutex
    err = ppdb_engine_mutex_unlock(mutex);
    ASSERT_OK(err);

    // Destroy mutex
    ppdb_engine_mutex_destroy(mutex);
}

#define NUM_THREADS 4
#define NUM_ITERATIONS 100
#define TEST_TIMEOUT_SEC 5

static atomic_uint counter = 0;
static atomic_uint active_threads = 0;
static ppdb_engine_mutex_t* shared_mutex = NULL;
static bool test_timeout = false;

static void* thread_func(void* arg) {
    int thread_id = (int)(intptr_t)arg;
    printf("Thread %d started\n", thread_id);
    
    atomic_fetch_add(&active_threads, 1);

    for (int i = 0; i < NUM_ITERATIONS && !test_timeout; i++) {
        ppdb_error_t err = ppdb_engine_mutex_lock(shared_mutex);
        if (err != PPDB_OK) {
            printf("Thread %d: Lock failed with error %d\n", thread_id, err);
            break;
        }

        uint32_t value = atomic_load(&counter);
        sched_yield();
        atomic_store(&counter, value + 1);

        err = ppdb_engine_mutex_unlock(shared_mutex);
        if (err != PPDB_OK) {
            printf("Thread %d: Unlock failed with error %d\n", thread_id, err);
            break;
        }

        if (i % 10 == 0) {
            printf("Thread %d: Progress %d%%\n", thread_id, (i * 100) / NUM_ITERATIONS);
        }
    }

    atomic_fetch_sub(&active_threads, 1);
    printf("Thread %d finished\n", thread_id);
    return NULL;
}

static void* timeout_thread(void* arg) {
    (void)arg;
    sleep(TEST_TIMEOUT_SEC);
    if (atomic_load(&active_threads) > 0) {
        printf("Test timeout after %d seconds! %u threads still active\n", 
            TEST_TIMEOUT_SEC, atomic_load(&active_threads));
        test_timeout = true;
    }
    return NULL;
}

void test_mutex_concurrent(void) {
    ppdb_error_t err;
    pthread_t threads[NUM_THREADS];
    pthread_t timeout_thread_handle;

    printf("Starting concurrent mutex test with %d threads, %d iterations each\n",
        NUM_THREADS, NUM_ITERATIONS);

    // Create shared mutex
    err = ppdb_engine_mutex_create(&shared_mutex);
    ASSERT_OK(err);
    ASSERT_NOT_NULL(shared_mutex);

    // Reset counter and flags
    atomic_store(&counter, 0);
    atomic_store(&active_threads, 0);
    test_timeout = false;

    // Start timeout thread
    pthread_create(&timeout_thread_handle, NULL, timeout_thread, NULL);

    // Create worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_func, (void*)(intptr_t)i);
    }

    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Check if test timed out
    if (test_timeout) {
        fprintf(stderr, "Test timed out!\n");
        exit(1);
    }

    // Wait for timeout thread
    pthread_join(timeout_thread_handle, NULL);

    // Verify counter
    uint32_t expected = NUM_THREADS * NUM_ITERATIONS;
    uint32_t actual = atomic_load(&counter);
    printf("Counter value: %u (expected: %u)\n", actual, expected);
    ASSERT_EQ(actual, expected);

    // Destroy mutex
    ppdb_engine_mutex_destroy(shared_mutex);
    shared_mutex = NULL;
}

int main(int argc, char* argv[]) {
    (void)argc;  // Unused parameter
    (void)argv;  // Unused parameter

    TEST_SUITE_BEGIN("Sync Tests");

    TEST_RUN(test_mutex_basic);
    TEST_RUN(test_mutex_concurrent);

    TEST_SUITE_END();
    return 0;
}
