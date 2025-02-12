#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
#include <string.h>

// Error codes
#define INFRAX_ERROR_CORE_INIT_FAILED -1001
#define INFRAX_ERROR_SYNC_CREATE_FAILED -1002

// Test parameters
#define TEST_PORT_TCP 22345
#define TEST_PORT_UDP 22346
#define TEST_TIMEOUT_MS 5000
#define TEST_BUFFER_SIZE 4096

// Global variables
static InfraxCore* core = NULL;
static InfraxSync* server_mutex = NULL;
static InfraxSync* server_cond = NULL;
static bool tcp_server_ready = false;
static bool tcp_server_running = false;
static bool udp_server_ready = false;
static bool udp_server_running = false;

// Initialize core and addresses
static InfraxError init_test_env(void) {
    // Initialize core
    core = InfraxCoreClass.singleton();
    if (!core) {
        printf("Failed to initialize core!\n");
        return make_error(INFRAX_ERROR_CORE_INIT_FAILED, "Failed to initialize core");
    }

    // Create synchronization primitives
    server_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    server_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    
    if (!server_mutex || !server_cond) {
        if (server_mutex) InfraxSyncClass.free(server_mutex);
        if (server_cond) InfraxSyncClass.free(server_cond);
        return make_error(INFRAX_ERROR_SYNC_CREATE_FAILED, "Failed to create sync primitives");
    }

    return INFRAX_ERROR_OK_STRUCT;
}

// TCP server thread function
static void* tcp_server_thread(void* arg) {
    (void)arg;
    InfraxError err;
    InfraxSocket* server = NULL;
    InfraxSocket* client = NULL;
    char buffer[TEST_BUFFER_SIZE];

    // Create server socket
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS,
        .reuse_addr = true
    };

    server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "TCP server: Failed to create socket\n");
        return NULL;
    }

    // Bind server
    InfraxNetAddr addr;
    err = infrax_net_addr_from_string("127.0.0.1", TEST_PORT_TCP, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "TCP server: Failed to create address: %s\n", err.message);
        goto cleanup;
    }

    err = server->bind(server, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "TCP server: Failed to bind: %s\n", err.message);
        goto cleanup;
    }

    err = server->listen(server, 5);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "TCP server: Failed to listen: %s\n", err.message);
        goto cleanup;
    }

    // Signal ready
    server_mutex->mutex_lock(server_mutex);
    tcp_server_ready = true;
    server_cond->cond_signal(server_cond);
    server_mutex->mutex_unlock(server_mutex);

    core->printf(core, "TCP server: Ready and listening\n");

    // Main server loop
    while (tcp_server_running) {
        InfraxNetAddr client_addr;
        err = server->accept(server, &client, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (!tcp_server_running) break;
            core->printf(core, "TCP server: Accept failed: %s\n", err.message);
            continue;
        }

        core->printf(core, "TCP server: Client connected from %s:%d\n", 
                    client_addr.ip, client_addr.port);

        // Echo loop
        while (tcp_server_running) {
            size_t received;
            err = client->recv(client, buffer, sizeof(buffer), &received);
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "TCP server: Receive error: %s\n", err.message);
                break;
            }

            if (received == 0) {
                core->printf(core, "TCP server: Client disconnected\n");
                break;
            }

            size_t sent;
            err = client->send(client, buffer, received, &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "TCP server: Send error: %s\n", err.message);
                break;
            }

            core->printf(core, "TCP server: Echoed %zu bytes\n", sent);
        }

        if (client) {
            InfraxSocketClass.free(client);
            client = NULL;
        }
    }

cleanup:
    if (client) InfraxSocketClass.free(client);
    if (server) InfraxSocketClass.free(server);
    return NULL;
}

// UDP server thread function
static void* udp_server_thread(void* arg) {
    (void)arg;
    InfraxError err;
    InfraxSocket* server = NULL;
    char buffer[TEST_BUFFER_SIZE];

    // Create server socket
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS,
        .reuse_addr = true
    };

    server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "UDP server: Failed to create socket\n");
        return NULL;
    }

    // Bind server
    InfraxNetAddr addr;
    err = infrax_net_addr_from_string("127.0.0.1", TEST_PORT_UDP, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "UDP server: Failed to create address: %s\n", err.message);
        goto cleanup;
    }

    err = server->bind(server, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "UDP server: Failed to bind: %s\n", err.message);
        goto cleanup;
    }

    // Signal ready
    server_mutex->mutex_lock(server_mutex);
    udp_server_ready = true;
    server_cond->cond_signal(server_cond);
    server_mutex->mutex_unlock(server_mutex);

    core->printf(core, "UDP server: Ready and listening\n");

    // Main server loop
    while (udp_server_running) {
        InfraxNetAddr client_addr;
        size_t received;
        err = server->recvfrom(server, buffer, sizeof(buffer), &received, &client_addr);
        
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                continue;
            }
            core->printf(core, "UDP server: Receive error: %s\n", err.message);
            continue;
        }

        if (received == 0) continue;

        core->printf(core, "UDP server: Received %zu bytes from %s:%d\n",
                    received, client_addr.ip, client_addr.port);

        size_t sent;
        err = server->sendto(server, buffer, received, &sent, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "UDP server: Send error: %s\n", err.message);
            continue;
        }

        core->printf(core, "UDP server: Sent %zu bytes back\n", sent);
    }

cleanup:
    if (server) InfraxSocketClass.free(server);
    return NULL;
}

// Test TCP functionality
static int test_tcp(void) {
    InfraxError err;
    InfraxSocket* client = NULL;
    const char* test_data = "Hello, TCP!";
    char buffer[TEST_BUFFER_SIZE];
    int ret = -1;

    core->printf(core, "Testing TCP...\n");

    // Create client socket
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS,
        .reuse_addr = false
    };

    client = InfraxSocketClass.new(&config);
    if (!client) {
        core->printf(core, "TCP test: Failed to create client socket\n");
        return -1;
    }

    // Connect to server
    InfraxNetAddr server_addr;
    err = infrax_net_addr_from_string("127.0.0.1", TEST_PORT_TCP, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "TCP test: Failed to create server address: %s\n", err.message);
        goto cleanup;
    }

    err = client->connect(client, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "TCP test: Failed to connect: %s\n", err.message);
        goto cleanup;
    }

    // Send data
    size_t data_len = strlen(test_data);
    size_t sent;
    err = client->send(client, test_data, data_len, &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "TCP test: Failed to send: %s\n", err.message);
        goto cleanup;
    }

    core->printf(core, "TCP test: Sent %zu bytes\n", sent);

    // Receive echo
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "TCP test: Failed to receive: %s\n", err.message);
        goto cleanup;
    }

    core->printf(core, "TCP test: Received %zu bytes\n", received);

    // Verify data
    if (received != data_len || memcmp(test_data, buffer, data_len) != 0) {
        core->printf(core, "TCP test: Data verification failed\n");
        goto cleanup;
    }

    core->printf(core, "TCP test passed\n");
    ret = 0;

cleanup:
    if (client) InfraxSocketClass.free(client);
    return ret;
}

// Test UDP functionality
static int test_udp(void) {
    InfraxError err;
    InfraxSocket* client = NULL;
    const char* test_data = "Hello, UDP!";
    char buffer[TEST_BUFFER_SIZE];
    int ret = -1;

    core->printf(core, "Testing UDP...\n");

    // Create client socket
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS,
        .reuse_addr = false
    };

    client = InfraxSocketClass.new(&config);
    if (!client) {
        core->printf(core, "UDP test: Failed to create client socket\n");
        return -1;
    }

    // Send data to server
    InfraxNetAddr server_addr;
    err = infrax_net_addr_from_string("127.0.0.1", TEST_PORT_UDP, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "UDP test: Failed to create server address: %s\n", err.message);
        goto cleanup;
    }

    size_t data_len = strlen(test_data);
    size_t sent;
    err = client->sendto(client, test_data, data_len, &sent, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "UDP test: Failed to send: %s\n", err.message);
        goto cleanup;
    }

    core->printf(core, "UDP test: Sent %zu bytes\n", sent);

    // Receive echo
    InfraxNetAddr recv_addr;
    size_t received;
    err = client->recvfrom(client, buffer, sizeof(buffer), &received, &recv_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "UDP test: Failed to receive: %s\n", err.message);
        goto cleanup;
    }

    core->printf(core, "UDP test: Received %zu bytes from %s:%d\n",
                received, recv_addr.ip, recv_addr.port);

    // Verify data
    if (received != data_len || memcmp(test_data, buffer, data_len) != 0) {
        core->printf(core, "UDP test: Data verification failed\n");
        goto cleanup;
    }

    core->printf(core, "UDP test passed\n");
    ret = 0;

cleanup:
    if (client) InfraxSocketClass.free(client);
    return ret;
}

int main(void) {
    InfraxError err;
    InfraxThread* tcp_thread = NULL;
    InfraxThread* udp_thread = NULL;
    int ret = 1;

    // Initialize test environment
    err = init_test_env();
    if (INFRAX_ERROR_IS_ERR(err)) {
        printf("Failed to initialize test environment: %s\n", err.message);
        return 1;
    }

    core->printf(core, "Starting network tests...\n");

    // Start TCP server
    tcp_server_running = true;
    InfraxThreadConfig tcp_config = {
        .name = "tcp_server",
        .func = tcp_server_thread,
        .arg = NULL
    };

    tcp_thread = InfraxThreadClass.new(&tcp_config);
    if (!tcp_thread) {
        core->printf(core, "Failed to create TCP server thread\n");
        goto cleanup;
    }

    err = tcp_thread->start(tcp_thread, tcp_server_thread, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to start TCP server thread: %s\n", err.message);
        goto cleanup;
    }

    // Wait for TCP server to be ready
    server_mutex->mutex_lock(server_mutex);
    while (!tcp_server_ready) {
        err = server_cond->cond_timedwait(server_cond, server_mutex, TEST_TIMEOUT_MS);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Timeout waiting for TCP server\n");
            server_mutex->mutex_unlock(server_mutex);
            goto cleanup;
        }
    }
    server_mutex->mutex_unlock(server_mutex);

    // Start UDP server
    udp_server_running = true;
    InfraxThreadConfig udp_config = {
        .name = "udp_server",
        .func = udp_server_thread,
        .arg = NULL
    };

    udp_thread = InfraxThreadClass.new(&udp_config);
    if (!udp_thread) {
        core->printf(core, "Failed to create UDP server thread\n");
        goto cleanup;
    }

    err = udp_thread->start(udp_thread, udp_server_thread, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to start UDP server thread: %s\n", err.message);
        goto cleanup;
    }

    // Wait for UDP server to be ready
    server_mutex->mutex_lock(server_mutex);
    while (!udp_server_ready) {
        err = server_cond->cond_timedwait(server_cond, server_mutex, TEST_TIMEOUT_MS);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Timeout waiting for UDP server\n");
            server_mutex->mutex_unlock(server_mutex);
            goto cleanup;
        }
    }
    server_mutex->mutex_unlock(server_mutex);

    // Run tests
    if (test_tcp() != 0) {
        core->printf(core, "TCP test failed\n");
        goto cleanup;
    }

    if (test_udp() != 0) {
        core->printf(core, "UDP test failed\n");
        goto cleanup;
    }

    core->printf(core, "All tests passed!\n");
    ret = 0;

cleanup:
    // Stop servers
    tcp_server_running = false;
    udp_server_running = false;

    // Wait for server threads to finish
    if (tcp_thread) {
        void* thread_result;
        tcp_thread->join(tcp_thread, &thread_result);
        InfraxThreadClass.free(tcp_thread);
    }

    if (udp_thread) {
        void* thread_result;
        udp_thread->join(udp_thread, &thread_result);
        InfraxThreadClass.free(udp_thread);
    }

    // Free synchronization primitives
    if (server_mutex) InfraxSyncClass.free(server_mutex);
    if (server_cond) InfraxSyncClass.free(server_cond);

    return ret;
}
