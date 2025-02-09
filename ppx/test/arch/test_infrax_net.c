#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
// #include <assert.h> use assert from our core
#include <string.h> //todo use string func from our core
#include <time.h> //todo use time func from our core

static InfraxCore* core = NULL;
static InfraxSync* test_mutex = NULL;
static InfraxSync* test_cond = NULL;
static bool tcp_server_ready = false;
static bool udp_server_ready = false;

static void test_config() {
    if (!core) core = InfraxCoreClass.singleton();
    core->printf(core, "Testing socket configuration...\n");
    
    InfraxSocket* socket = NULL;
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    socket = InfraxSocketClass.new(&config);
    if (!socket) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "socket != NULL", "Failed to create TCP socket");
        return;
    }
    InfraxSocketClass.free(socket);
    
    // Test UDP configuration
    config.is_udp = true;
    socket = InfraxSocketClass.new(&config);
    if (!socket) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "socket != NULL", "Failed to create UDP socket");
        return;
    }
    InfraxSocketClass.free(socket);
    
    // Test non-blocking configuration
    config.is_nonblocking = true;
    socket = InfraxSocketClass.new(&config);
    if (!socket) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "socket != NULL", "Failed to create non-blocking UDP socket");
        return;
    }
    InfraxSocketClass.free(socket);
    
    core->printf(core, "Socket configuration tests passed\n");
}

static void* tcp_server_thread(void* arg) {
    if (!core) core = InfraxCoreClass.singleton();
    (void)arg;
    InfraxSocket* server = NULL;
    InfraxSocket* client = NULL;
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 5000,  // 5 seconds timeout
        .recv_timeout_ms = 5000   // 5 seconds timeout
    };
    
    server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "Failed to create TCP server socket\n");
        return NULL;
    }
    
    InfraxNetAddr addr;
    core->strcpy(core, addr.ip, "127.0.0.1");
    addr.port = 9090;
    
    InfraxError err = server->bind(server, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to bind TCP server socket: %s\n", err.message);
        goto cleanup;
    }
    
    err = server->listen(server, 5);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to listen on TCP server socket: %s\n", err.message);
        goto cleanup;
    }
    
    // Signal that server is ready
    err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in TCP server: %s\n", err.message);
        goto cleanup;
    }
    
    tcp_server_ready = true;
    err = test_cond->cond_signal(test_cond);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to signal condition in TCP server: %s\n", err.message);
        test_mutex->mutex_unlock(test_mutex);
        goto cleanup;
    }
    
    err = test_mutex->mutex_unlock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to unlock mutex in TCP server: %s\n", err.message);
        goto cleanup;
    }
    
    // Accept client connection with timeout
    err = server->accept(server, &client, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to accept client connection: %s\n", err.message);
        goto cleanup;
    }
    
    // Echo data back to client
    char buffer[256];
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to receive data from client: %s\n", err.message);
        goto cleanup;
    }
    
    size_t sent;
    err = client->send(client, buffer, received, &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to send data back to client: %s\n", err.message);
        goto cleanup;
    }
    
cleanup:
    if (client) {
        InfraxSocketClass.free(client);
    }
    if (server) {
        InfraxSocketClass.free(server);
    }
    return NULL;
}

static void* udp_server_thread(void* arg) {
    if (!core) core = InfraxCoreClass.singleton();
    (void)arg;
    InfraxSocket* server = NULL;
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 5000,  // 5 seconds timeout
        .recv_timeout_ms = 5000   // 5 seconds timeout
    };
    
    server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "Failed to create UDP server socket\n");
        return NULL;
    }
    
    InfraxNetAddr addr;
    core->strcpy(core, addr.ip, "127.0.0.1");
    addr.port = 8081;
    
    InfraxError err = server->bind(server, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to bind UDP server socket: %s\n", err.message);
        goto cleanup;
    }
    
    // Signal that server is ready
    err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in UDP server: %s\n", err.message);
        goto cleanup;
    }
    
    udp_server_ready = true;
    err = test_cond->cond_signal(test_cond);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to signal condition in UDP server: %s\n", err.message);
        test_mutex->mutex_unlock(test_mutex);
        goto cleanup;
    }
    
    err = test_mutex->mutex_unlock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to unlock mutex in UDP server: %s\n", err.message);
        goto cleanup;
    }
    
    // Receive and echo data
    char buffer[256];
    size_t received;
    err = server->recv(server, buffer, sizeof(buffer), &received);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to receive data in UDP server: %s\n", err.message);
        goto cleanup;
    }
    
    size_t sent;
    err = server->send(server, buffer, received, &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to send data back in UDP server: %s\n", err.message);
        goto cleanup;
    }
    
cleanup:
    if (server) {
        InfraxSocketClass.free(server);
    }
    return NULL;
}

static void test_tcp() {
    if (!core) core = InfraxCoreClass.singleton();
    core->printf(core, "Testing TCP socket operations...\n");
    
    // Create server thread
    InfraxThreadConfig thread_config = {
        .name = "tcp_server",
        .entry_point = tcp_server_thread,
        .arg = NULL
    };
    InfraxThread* server_thread = InfraxThreadClass.new(&thread_config);
    if (!server_thread) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "server_thread != NULL", "Failed to create server thread");
        return;
    }
    
    InfraxError err = server_thread->start(server_thread);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    
    // Wait for server to be ready with timeout
    err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    
    struct timespec timeout;
    core->time_now_ms(core); // 获取当前时间
    
    while (!tcp_server_ready) {
        err = test_cond->cond_timedwait(test_cond, test_mutex, 5000); // 5 seconds timeout
        if (err.code == INFRAX_ERROR_SYNC_TIMEOUT) {
            core->printf(core, "Timeout waiting for TCP server to be ready\n");
            test_mutex->mutex_unlock(test_mutex);
            goto cleanup;
        }
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
            test_mutex->mutex_unlock(test_mutex);
            goto cleanup;
        }
    }
    err = test_mutex->mutex_unlock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    tcp_server_ready = false;
    
    // Create client socket
    InfraxSocket* client = NULL;
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    client = InfraxSocketClass.new(&config);
    if (!client) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "client != NULL", "Failed to create client socket");
        goto cleanup;
    }
    
    InfraxNetAddr addr;
    core->strcpy(core, addr.ip, "127.0.0.1");
    addr.port = 9090;  // Connect to the new server port
    
    err = client->connect(client, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    
    // Send and receive data
    const char* test_data = "Hello, TCP!";
    size_t sent;
    err = client->send(client, test_data, core->strlen(core, test_data), &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    if (sent != core->strlen(core, test_data)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "sent == strlen(test_data)", "Data length mismatch");
        goto cleanup;
    }
    
    char buffer[256];
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    if (received != sent) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "received == sent", "Data length mismatch");
        goto cleanup;
    }
    buffer[received] = '\0';  // Null terminate the buffer for strcmp
    if (core->strcmp(core, buffer, test_data) != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "strcmp(buffer, test_data) == 0", "Data content mismatch");
        goto cleanup;
    }
    
    InfraxSocketClass.free(client);
    client = NULL;
    
    err = server_thread->join(server_thread, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    
    InfraxThreadClass.free(server_thread);
    server_thread = NULL;
    
    core->printf(core, "TCP socket tests completed\n");
    return;

cleanup:
    if (client) {
        InfraxSocketClass.free(client);
    }
    if (server_thread) {
        err = server_thread->join(server_thread, NULL);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        }
        InfraxThreadClass.free(server_thread);
    }
    
    core->printf(core, "TCP socket tests completed\n");
}

static void test_udp() {
    if (!core) core = InfraxCoreClass.singleton();
    core->printf(core, "Testing UDP socket operations...\n");
    
    // Create server thread
    InfraxThreadConfig thread_config = {
        .name = "udp_server",
        .entry_point = udp_server_thread,
        .arg = NULL
    };
    InfraxThread* server_thread = InfraxThreadClass.new(&thread_config);
    if (!server_thread) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "server_thread != NULL", "Failed to create server thread");
        return;
    }
    
    InfraxError err = server_thread->start(server_thread);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    
    // Wait for server to be ready with timeout
    err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    
    struct timespec timeout;
    core->time_now_ms(core); // 获取当前时间
    
    while (!udp_server_ready) {
        err = test_cond->cond_timedwait(test_cond, test_mutex, 5000); // 5 seconds timeout
        if (err.code == INFRAX_ERROR_SYNC_TIMEOUT) {
            core->printf(core, "Timeout waiting for UDP server to be ready\n");
            test_mutex->mutex_unlock(test_mutex);
            goto cleanup;
        }
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
            test_mutex->mutex_unlock(test_mutex);
            goto cleanup;
        }
    }
    err = test_mutex->mutex_unlock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    udp_server_ready = false;
    
    // Create client socket
    InfraxSocket* client = NULL;
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    client = InfraxSocketClass.new(&config);
    if (!client) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "client != NULL", "Failed to create client socket");
        goto cleanup;
    }
    
    // Set server address
    InfraxNetAddr addr;
    core->strcpy(core, addr.ip, "127.0.0.1");
    addr.port = 8081;  // Use the UDP server port
    client->peer_addr = addr;  // Set the peer address for sending
    
    // Send and receive data
    const char* test_data = "Hello, UDP!";
    size_t sent;
    err = client->send(client, test_data, core->strlen(core, test_data), &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    if (sent != core->strlen(core, test_data)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "sent == strlen(test_data)", "Data length mismatch");
        goto cleanup;
    }
    
    char buffer[256];
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    if (received != sent) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "received == sent", "Data length mismatch");
        goto cleanup;
    }
    buffer[received] = '\0';  // Null terminate the buffer for strcmp
    if (core->strcmp(core, buffer, test_data) != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "strcmp(buffer, test_data) == 0", "Data content mismatch");
        goto cleanup;
    }
    
    InfraxSocketClass.free(client);
    client = NULL;
    
    err = server_thread->join(server_thread, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        goto cleanup;
    }
    
    InfraxThreadClass.free(server_thread);
    server_thread = NULL;
    
    core->printf(core, "UDP socket tests completed\n");
    return;

cleanup:
    if (client) {
        InfraxSocketClass.free(client);
    }
    if (server_thread) {
        err = server_thread->join(server_thread, NULL);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "INFRAX_ERROR_IS_OK(err)", err.message);
        }
        InfraxThreadClass.free(server_thread);
    }
    
    core->printf(core, "UDP socket tests completed\n");
}

int main() {
    // Initialize core first
    core = InfraxCoreClass.singleton();
    if (!core) {
        printf("Failed to initialize InfraxCore\n");
        return 1;
    }
    
    core->printf(core, "===================\nStarting InfraxNet tests...\n");
    
    // Initialize test mutex
    test_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    if (test_mutex == NULL) {
        core->printf(core, "Failed to create test mutex\n");
        return 1;
    }

    // Initialize test condition variable
    test_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    if (test_cond == NULL) {
        core->printf(core, "Failed to create test condition variable\n");
        InfraxSyncClass.free(test_mutex);
        return 1;
    }
    
    test_config();
    test_tcp();
    test_udp();
    
    // Clean up
    InfraxSyncClass.free(test_mutex);
    InfraxSyncClass.free(test_cond);
    
    core->printf(core, "All InfraxNet tests passed!\n");
    return 0;
}
