#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_io.h"
#include "internal/infra/infra_event.h"

static int test_count = 0;
static int fail_count = 0;

#define TEST_ASSERT(cond, msg) do { \
    test_count++; \
    if (!(cond)) { \
        printf("FAIL: %s\n", msg); \
        fail_count++; \
        return -1; \
    } \
} while(0)

static void test_timer_handler(void* ctx, int events) {
    int* counter = (int*)ctx;
    (*counter)++;
}

static void test_io_handler(int fd, void* arg) {
    int* counter = (int*)arg;
    (*counter)++;
}

static int test_infra_init(void) {
    printf("Testing infra initialization...\n");
    fflush(stdout);

    // Test memory management
    void* ptr = infra_malloc(42);
    TEST_ASSERT(ptr != NULL, "Memory allocation failed");
    infra_free(ptr);

    // Test event loop initialization
    struct infra_event_loop loop;
    TEST_ASSERT(infra_event_loop_init(&loop) == 0, "Event loop initialization failed");
    infra_event_loop_destroy(&loop);

    printf("Infra initialization test passed\n");
    fflush(stdout);
    return 0;
}

static int test_event_loop(void) {
    printf("Testing event loop...\n");
    fflush(stdout);

    struct infra_event_loop loop;
    TEST_ASSERT(infra_event_loop_init(&loop) == 0, "Event loop initialization failed");

    // Test timer
    int timer_counter = 0;
    struct infra_timer timer;
    u64 deadline = time(NULL) + 1; // 1 second from now
    
    TEST_ASSERT(infra_timer_init(&timer, deadline, test_timer_handler, &timer_counter) == 0,
                "Timer initialization failed");
    TEST_ASSERT(infra_timer_add(&loop, &timer) == 0, "Timer add failed");

    // Test IO
    int io_counter = 0;
    int pipefd[2];
    TEST_ASSERT(pipe(pipefd) == 0, "Pipe creation failed");

    TEST_ASSERT(event_add_io(&loop, pipefd[0], EVENT_READ, test_io_handler, &io_counter) == 0,
                "IO event add failed");

    // Write to pipe to trigger IO event
    char data = 'x';
    TEST_ASSERT(write(pipefd[1], &data, 1) == 1, "Write to pipe failed");

    // Run event loop for 2 seconds
    struct infra_timer stop_timer;
    deadline = time(NULL) + 2;
    TEST_ASSERT(infra_timer_init(&stop_timer, deadline, test_timer_handler, &loop) == 0,
                "Stop timer initialization failed");
    TEST_ASSERT(infra_timer_add(&loop, &stop_timer) == 0, "Stop timer add failed");

    TEST_ASSERT(infra_event_loop_run(&loop) == 0, "Event loop run failed");

    // Verify results
    TEST_ASSERT(timer_counter > 0, "Timer did not fire");
    TEST_ASSERT(io_counter > 0, "IO event did not fire");

    // Cleanup
    close(pipefd[0]);
    close(pipefd[1]);
    infra_event_loop_destroy(&loop);

    printf("Event loop test passed\n");
    fflush(stdout);
    return 0;
}

static int test_main(void) {
    printf("Running infra tests...\n");
    fflush(stdout);
    
    int result = 0;
    result |= test_infra_init();
    result |= test_event_loop();

    printf("Test completed with result: %d\n", result);
    printf("Total tests: %d, Failed: %d\n", test_count, fail_count);
    printf("Test %s\n", result == 0 ? "PASSED" : "FAILED");
    fflush(stdout);
    return result;
}

COSMOPOLITAN_C_START_
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return test_main();
}
COSMOPOLITAN_C_END_ 