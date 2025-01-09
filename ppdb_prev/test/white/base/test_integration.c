#include <cosmopolitan.h>
#include "internal/base.h"

// Test data
static const int TEST_PORT = 12345;
static const int NUM_CLIENTS = 100;
static const int NUM_THREADS = 4;
static _Atomic(int) active_clients = 0;
static _Atomic(uint64_t) total_requests = 0;

// Client thread function
static void client_thread_func(void* arg) {
    int thread_id = *(int*)arg;
    int clients_per_thread = NUM_CLIENTS / NUM_THREADS;
    
    for (int i = 0; i < clients_per_thread; i++) {
        // Create client socket
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_TRUE(client_fd >= 0);
        
        // Connect to server
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TEST_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ASSERT_TRUE(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
        
        atomic_fetch_add(&active_clients, 1);
        
        // Send multiple requests
        for (int j = 0; j < 10; j++) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "Request %d from client %d-%d", j, thread_id, i);
            ASSERT_TRUE(write(client_fd, buffer, strlen(buffer)) > 0);
            atomic_fetch_add(&total_requests, 1);
            ppdb_base_sleep(rand() % 10); // Random delay
        }
        
        // Close connection
        close(client_fd);
        atomic_fetch_sub(&active_clients, 1);
    }
}

// Server thread function
static void server_thread_func(void* arg) {
    ppdb_net_server_t* server = (ppdb_net_server_t*)arg;
    ppdb_base_skiplist_t* request_log = NULL;
    
    // Create request log
    ASSERT_OK(ppdb_base_skiplist_init(&request_log));
    
    // Create timer for cleanup
    ppdb_base_timer_t* cleanup_timer = NULL;
    ASSERT_OK(ppdb_base_timer_create(&cleanup_timer, 1000)); // 1 second interval
    cleanup_timer->repeating = true;
    
    while (atomic_load(&active_clients) > 0) {
        // Accept new connection
        ppdb_connection_t* conn = NULL;
        ppdb_error_t err = ppdb_net_server_accept(server, &conn);
        if (err == PPDB_OK && conn) {
            // Set timeout
            ppdb_net_set_connection_timeout(conn, 5000); // 5 seconds
            
            // Handle connection events
            handle_connection_event(conn);
            
            // Log request
            uint64_t now;
            ppdb_base_time_get_microseconds(&now);
            ppdb_base_skiplist_insert(request_log, &now, sizeof(now), conn, sizeof(ppdb_connection_t));
        }
        
        // Update timers
        ppdb_base_timer_update();
        
        ppdb_base_sleep(1);
    }
    
    // Cleanup
    ppdb_base_timer_destroy(cleanup_timer);
    ppdb_base_skiplist_destroy(request_log);
}

// Test system integration
static int test_system_integration(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_base_thread_t* server_thread = NULL;
    ppdb_base_thread_t* client_threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    uint64_t start_time, end_time;
    
    // Create server
    ASSERT_OK(ppdb_net_server_create(&server, TEST_PORT));
    
    // Reset counters
    atomic_store(&active_clients, 0);
    atomic_store(&total_requests, 0);
    
    // Start server thread
    ASSERT_OK(ppdb_base_thread_create(&server_thread, server_thread_func, server));
    
    // Record start time
    ASSERT_OK(ppdb_base_time_get_microseconds(&start_time));
    
    // Start client threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        ASSERT_OK(ppdb_base_thread_create(&client_threads[i], client_thread_func, &thread_ids[i]));
    }
    
    // Wait for client threads
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_OK(ppdb_base_thread_join(client_threads[i]));
        ASSERT_OK(ppdb_base_thread_destroy(client_threads[i]));
    }
    
    // Wait for server thread
    ASSERT_OK(ppdb_base_thread_join(server_thread));
    ASSERT_OK(ppdb_base_thread_destroy(server_thread));
    
    // Record end time
    ASSERT_OK(ppdb_base_time_get_microseconds(&end_time));
    
    // Print statistics
    uint64_t total_time = end_time - start_time;
    uint64_t requests = atomic_load(&total_requests);
    printf("Total requests: %lu\n", requests);
    printf("Total time: %lu us\n", total_time);
    printf("Average request time: %lu us/request\n", total_time / requests);
    
    // Cleanup
    ASSERT_OK(ppdb_net_server_destroy(server));
    return 0;
}

// Test error handling
static int test_error_handling(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_connection_t* conn = NULL;
    ppdb_base_skiplist_t* list = NULL;
    ppdb_base_timer_t* timer = NULL;
    
    // Test invalid parameters
    ASSERT_ERROR(ppdb_net_server_create(NULL, TEST_PORT));
    ASSERT_ERROR(ppdb_net_server_accept(NULL, &conn));
    ASSERT_ERROR(ppdb_base_skiplist_init(NULL));
    ASSERT_ERROR(ppdb_base_timer_create(NULL, 1000));
    
    // Test invalid operations
    ASSERT_ERROR(ppdb_net_get_connection_state(NULL, NULL));
    ASSERT_ERROR(ppdb_base_skiplist_insert(NULL, NULL, 0, NULL, 0));
    ASSERT_ERROR(ppdb_base_timer_update());
    
    return 0;
}

// Performance test
static int test_system_performance(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_base_skiplist_t* list = NULL;
    ppdb_base_timer_t* timer = NULL;
    uint64_t start_time, end_time;
    
    // Initialize components
    ASSERT_OK(ppdb_net_server_create(&server, TEST_PORT));
    ASSERT_OK(ppdb_base_skiplist_init(&list));
    ASSERT_OK(ppdb_base_timer_create(&timer, 1000));
    
    // Measure server performance
    ASSERT_OK(ppdb_base_time_get_microseconds(&start_time));
    for (int i = 0; i < 1000; i++) {
        ppdb_connection_t* conn = NULL;
        ppdb_net_server_accept(server, &conn);
        if (conn) cleanup_connection(conn);
    }
    ASSERT_OK(ppdb_base_time_get_microseconds(&end_time));
    printf("Server accept time: %lu us/conn\n", (end_time - start_time) / 1000);
    
    // Measure skiplist performance
    ASSERT_OK(ppdb_base_time_get_microseconds(&start_time));
    for (int i = 0; i < 10000; i++) {
        uint64_t key = i;
        ppdb_base_skiplist_insert(list, &key, sizeof(key), &key, sizeof(key));
    }
    ASSERT_OK(ppdb_base_time_get_microseconds(&end_time));
    printf("Skiplist insert time: %lu us/op\n", (end_time - start_time) / 10000);
    
    // Measure timer performance
    ASSERT_OK(ppdb_base_time_get_microseconds(&start_time));
    for (int i = 0; i < 1000; i++) {
        ppdb_base_timer_update();
    }
    ASSERT_OK(ppdb_base_time_get_microseconds(&end_time));
    printf("Timer update time: %lu us/op\n", (end_time - start_time) / 1000);
    
    // Cleanup
    ASSERT_OK(ppdb_net_server_destroy(server));
    ASSERT_OK(ppdb_base_skiplist_destroy(list));
    ASSERT_OK(ppdb_base_timer_destroy(timer));
    return 0;
}

int main(void) {
    printf("Testing system integration...\n");
    TEST_RUN(test_system_integration);
    printf("PASSED\n");
    
    printf("Testing error handling...\n");
    TEST_RUN(test_error_handling);
    printf("PASSED\n");
    
    printf("Testing system performance...\n");
    TEST_RUN(test_system_performance);
    printf("PASSED\n");
    
    return 0;
} 