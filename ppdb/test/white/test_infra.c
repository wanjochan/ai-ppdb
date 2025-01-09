#include "cosmopolitan.h"
#include "internal/infra/infra.h"

// Test event loop creation and destruction
static void test_event_loop(void) {
    infra_event_loop_t* loop = NULL;
    int ret = infra_event_loop_create(&loop);
    assert(ret == 0);
    assert(loop != NULL);
    assert(loop->running == false);
    assert(loop->events == NULL);
    assert(loop->event_count == 0);
    assert(loop->epoll_fd >= 0);
    
    ret = infra_event_loop_destroy(loop);
    assert(ret == 0);
}

// Test timer creation and basic operations
static void test_timer(void) {
    infra_event_loop_t* loop = NULL;
    int ret = infra_event_loop_create(&loop);
    assert(ret == 0);
    
    infra_timer_t* timer = NULL;
    ret = infra_timer_create(loop, &timer, 1000); // 1 second timer
    assert(ret == 0);
    assert(timer != NULL);
    assert(timer->interval_ms == 1000);
    assert(timer->repeating == false);
    
    ret = infra_timer_start(loop, timer, true); // Start repeating timer
    assert(ret == 0);
    assert(timer->repeating == true);
    
    ret = infra_timer_stop(loop, timer);
    assert(ret == 0);
    
    ret = infra_timer_destroy(loop, timer);
    assert(ret == 0);
    
    ret = infra_event_loop_destroy(loop);
    assert(ret == 0);
}

// Test event registration and modification
static void test_event(void) {
    infra_event_loop_t* loop = NULL;
    int ret = infra_event_loop_create(&loop);
    assert(ret == 0);
    
    // Create a pipe for testing
    int pipefd[2];
    ret = pipe(pipefd);
    assert(ret == 0);
    
    // Register read event
    infra_event_t event = {0};
    event.fd = pipefd[0];
    event.events = INFRA_EVENT_READ;
    ret = infra_event_add(loop, &event);
    assert(ret == 0);
    
    // Modify to read/write
    event.events = INFRA_EVENT_READ | INFRA_EVENT_WRITE;
    ret = infra_event_modify(loop, &event);
    assert(ret == 0);
    
    // Remove event
    ret = infra_event_remove(loop, &event);
    assert(ret == 0);
    
    // Cleanup
    close(pipefd[0]);
    close(pipefd[1]);
    
    ret = infra_event_loop_destroy(loop);
    assert(ret == 0);
}

int main(void) {
    test_event_loop();
    test_timer();
    test_event();
    printf("All tests passed!\n");
    return 0;
} 