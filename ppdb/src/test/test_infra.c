#include "internal/infra/infra.h"
#include "libc/stdio/stdio.h"
#include "libc/mem/mem.h"

static void test_timer_callback(infra_timer_t* timer, void* user_data) {
    int* count = (int*)user_data;
    (*count)++;
}

static void test_io_callback(infra_event_t* event, uint32_t events) {
    int* count = (int*)event->user_data;
    (*count)++;
}

// Test event loop creation and destruction
static int test_event_loop() {
    infra_event_loop_t* loop = NULL;
    
    if (infra_event_loop_create(&loop) < 0) {
        printf("Failed to create event loop\n");
        return -1;
    }
    
    if (!loop) {
        printf("Event loop is NULL\n");
        return -1;
    }
    
    if (infra_event_loop_destroy(loop) < 0) {
        printf("Failed to destroy event loop\n");
        return -1;
    }
    
    printf("Event loop test passed\n");
    return 0;
}

// Test timer creation and callbacks
static int test_timer() {
    infra_event_loop_t* loop = NULL;
    infra_timer_t* timer = NULL;
    int count = 0;
    
    if (infra_event_loop_create(&loop) < 0) {
        printf("Failed to create event loop\n");
        return -1;
    }
    
    if (infra_timer_create(loop, &timer, 100) < 0) {
        printf("Failed to create timer\n");
        infra_event_loop_destroy(loop);
        return -1;
    }
    
    timer->callback = test_timer_callback;
    timer->user_data = &count;
    
    if (infra_timer_start(loop, timer, true) < 0) {
        printf("Failed to start timer\n");
        infra_timer_destroy(loop, timer);
        infra_event_loop_destroy(loop);
        return -1;
    }
    
    // Run loop for 500ms to get ~5 callbacks
    infra_event_loop_run(loop, 500);
    
    if (count < 4 || count > 6) {
        printf("Timer callback count incorrect: %d\n", count);
        infra_timer_destroy(loop, timer);
        infra_event_loop_destroy(loop);
        return -1;
    }
    
    if (infra_timer_stop(loop, timer) < 0) {
        printf("Failed to stop timer\n");
        infra_timer_destroy(loop, timer);
        infra_event_loop_destroy(loop);
        return -1;
    }
    
    infra_timer_destroy(loop, timer);
    infra_event_loop_destroy(loop);
    
    printf("Timer test passed\n");
    return 0;
}

// Test IO operations
static int test_io() {
    infra_event_loop_t* loop = NULL;
    int pipefd[2];
    char buf[128];
    int read_count = 0;
    int write_count = 0;
    
    if (pipe(pipefd) < 0) {
        printf("Failed to create pipe\n");
        return -1;
    }
    
    if (infra_event_loop_create(&loop) < 0) {
        printf("Failed to create event loop\n");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    // Test async write
    if (infra_io_write_async(loop, pipefd[1], "test", 4, test_io_callback) < 0) {
        printf("Failed to start async write\n");
        infra_event_loop_destroy(loop);
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    // Test async read
    if (infra_io_read_async(loop, pipefd[0], buf, 4, test_io_callback) < 0) {
        printf("Failed to start async read\n");
        infra_event_loop_destroy(loop);
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    // Run loop briefly
    infra_event_loop_run(loop, 100);
    
    if (read_count != 1 || write_count != 1) {
        printf("IO callback counts incorrect: read=%d, write=%d\n", 
               read_count, write_count);
        infra_event_loop_destroy(loop);
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    infra_event_loop_destroy(loop);
    close(pipefd[0]);
    close(pipefd[1]);
    
    printf("IO test passed\n");
    return 0;
}

int main() {
    printf("Starting infra tests...\n");
    
    if (test_event_loop() < 0) {
        printf("Event loop test failed\n");
        return 1;
    }
    
    if (test_timer() < 0) {
        printf("Timer test failed\n");
        return 1;
    }
    
    if (test_io() < 0) {
        printf("IO test failed\n");
        return 1;
    }
    
    printf("All infra tests passed!\n");
    return 0;
} 