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
    printf("Timer fired with events: %d\n", events);
    fflush(stdout);
    int* counter = (int*)ctx;
    (*counter)++;
}

static void test_io_handler(int fd, void* arg) {
    printf("IO event fired on fd: %d\n", fd);
    fflush(stdout);
    int* counter = (int*)arg;
    (*counter)++;
    
    // Read and discard the data
    char buf[1];
    read(fd, buf, 1);
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
    u64 deadline = time(NULL); // 立即触发
    
    printf("Initializing timer with deadline: %llu\n", deadline);
    fflush(stdout);
    
    TEST_ASSERT(infra_timer_init(&timer, deadline, test_timer_handler, &timer_counter) == 0,
                "Timer initialization failed");
    TEST_ASSERT(infra_timer_add(&loop, &timer) == 0, "Timer add failed");

    // Test IO using socketpair
    int io_counter = 0;
    int socks[2];
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, socks) == 0, "Socket pair creation failed");

    printf("Created socket pair: %d, %d\n", socks[0], socks[1]);
    fflush(stdout);

    TEST_ASSERT(event_add_io(&loop, socks[0], EVENT_READ, test_io_handler, &io_counter) == 0,
                "IO event add failed");

    // Write to socket to trigger IO event
    char data = 'x';
    TEST_ASSERT(write(socks[1], &data, 1) == 1, "Write to socket failed");

    printf("Wrote data to socket\n");
    fflush(stdout);

    // Run event loop for a very short time
    struct infra_timer stop_timer;
    deadline = time(NULL); // 立即触发
    
    printf("Initializing stop timer with deadline: %llu\n", deadline);
    fflush(stdout);
    
    TEST_ASSERT(infra_timer_init(&stop_timer, deadline, test_timer_handler, &loop) == 0,
                "Stop timer initialization failed");
    TEST_ASSERT(infra_timer_add(&loop, &stop_timer) == 0, "Stop timer add failed");

    printf("Starting event loop...\n");
    fflush(stdout);

    TEST_ASSERT(infra_event_loop_run(&loop) == 0, "Event loop run failed");

    printf("Event loop finished\n");
    fflush(stdout);

    // Verify results
    TEST_ASSERT(timer_counter > 0, "Timer did not fire");
    TEST_ASSERT(io_counter > 0, "IO event did not fire");

    // Cleanup
    close(socks[0]);
    close(socks[1]);
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