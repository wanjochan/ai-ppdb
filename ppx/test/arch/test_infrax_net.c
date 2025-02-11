#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"

static InfraxCore* core = NULL;
static InfraxSync* test_mutex = NULL;
static InfraxSync* test_cond = NULL;
static bool tcp_server_ready = false;
static bool udp_server_ready = false;
static InfraxNetAddr tcp_server_addr;
static InfraxNetAddr udp_server_addr;

// 添加线程安全的初始化标志
static bool core_initialized = false;
static InfraxSync* core_mutex = NULL;

static void test_config() {
    if (!core) return;
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
    (void)arg;
    InfraxSocket* server = NULL;
    InfraxSocket* client = NULL;
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 5000,  // 5 seconds timeout
        .recv_timeout_ms = 5000,  // 5 seconds timeout
        .reuse_addr = true       // Add SO_REUSEADDR option
    };
    
    server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "Failed to create TCP server socket\n");
        return (void*)-1;
    }
    
    InfraxNetAddr addr;
    core->strcpy(core, addr.ip, "127.0.0.1");
    addr.port = 0;  // Use dynamic port
    
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
    
    // Get server's bound address before signaling
    err = server->get_local_addr(server, &tcp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to get local address: %s\n", err.message);
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
    (void)arg;
    InfraxSocket* server = NULL;
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 5000,  // 5 seconds timeout
        .recv_timeout_ms = 5000,  // 5 seconds timeout
        .reuse_addr = true        // Add SO_REUSEADDR option
    };
    
    server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "Failed to create UDP server socket\n");
        return NULL;
    }
    
    InfraxNetAddr addr;
    core->strcpy(core, addr.ip, "127.0.0.1");
    addr.port = 0;  // Use dynamic port
    
    InfraxError err = server->bind(server, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to bind UDP server socket: %s\n", err.message);
        goto cleanup;
    }
    
    // Get server's bound address before signaling
    err = server->get_local_addr(server, &udp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to get local address: %s\n", err.message);
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
    InfraxError err = {.code = INFRAX_ERROR_OK, .message = ""};
    tcp_server_ready = false;  // Initialize state
    
    // Start server thread
    InfraxThread* server_thread = NULL;
    InfraxThreadConfig thread_config = {
        .name = "tcp_server",
        .func = tcp_server_thread,
        .arg = NULL
    };
    server_thread = InfraxThreadClass.new(&thread_config);
    if (!server_thread) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "server_thread != NULL", "Failed to create server thread");
        goto cleanup;
    }
    
    err = server_thread->start(server_thread, tcp_server_thread, NULL);  // Fixed: pass the thread function
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to start server thread: %s\n", err.message);
        goto cleanup;
    }
    
    // Wait for server to be ready
    err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in client: %s\n", err.message);
        goto cleanup;
    }
    
    while (!tcp_server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait on condition in client: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            goto cleanup;
        }
    }
    
    err = test_mutex->mutex_unlock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to unlock mutex in client: %s\n", err.message);
        goto cleanup;
    }
    
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
    addr.port = tcp_server_addr.port;  // Use shared variable containing server port
    
    err = client->connect(client, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to connect to server: %s\n", err.message);
        goto cleanup;
    }
    
    // Send data
    const char* data = "Hello, server!";
    size_t data_len = core->strlen(core, data);
    size_t sent;
    
    err = client->send(client, data, data_len, &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to send data: %s\n", err.message);
        goto cleanup;
    }
    
    // Receive response
    char buffer[1024];
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to receive data: %s\n", err.message);
        goto cleanup;
    }
    
    // Verify response
    if (core->strncmp(core, buffer, data, data_len) != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "response matches sent data", "Received data does not match sent data");
        goto cleanup;
    }
    
cleanup:
    if (client) {
        InfraxSocketClass.free(client);
    }
    if (server_thread) {
        void* result;
        server_thread->join(server_thread, &result);
        InfraxThreadClass.free(server_thread);
    }
}

static void test_udp() {
    InfraxError err = {.code = INFRAX_ERROR_OK, .message = ""};
    udp_server_ready = false;  // Initialize state
    
    // Start server thread
    InfraxThread* server_thread = NULL;
    InfraxThreadConfig thread_config = {
        .name = "udp_server",
        .func = udp_server_thread,
        .arg = NULL
    };
    server_thread = InfraxThreadClass.new(&thread_config);
    if (!server_thread) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "server_thread != NULL", "Failed to create server thread");
        goto cleanup;
    }
    
    err = server_thread->start(server_thread, udp_server_thread, NULL);  // Fixed: pass the thread function
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to start server thread: %s\n", err.message);
        goto cleanup;
    }
    
    // Wait for server to be ready
    err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in client: %s\n", err.message);
        goto cleanup;
    }
    
    while (!udp_server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait on condition in client: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            goto cleanup;
        }
    }
    
    err = test_mutex->mutex_unlock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to unlock mutex in client: %s\n", err.message);
        goto cleanup;
    }
    
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
    addr.port = udp_server_addr.port;  // Use shared variable containing server port
    client->peer_addr = addr;  // Set the peer address for sending
    
    // Send data
    const char* data = "Hello, server!";
    size_t data_len = core->strlen(core, data);
    size_t sent;
    
    err = client->send(client, data, data_len, &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to send data: %s\n", err.message);
        goto cleanup;
    }
    
    // Receive response
    char buffer[1024];
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to receive data: %s\n", err.message);
        goto cleanup;
    }
    
    // Verify response
    if (core->strncmp(core, buffer, data, data_len) != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "response matches sent data", "Received data does not match sent data");
        goto cleanup;
    }
    
cleanup:
    if (client) {
        InfraxSocketClass.free(client);
    }
    if (server_thread) {
        void* result;
        server_thread->join(server_thread, &result);
        InfraxThreadClass.free(server_thread);
    }
}

int main() {
    core_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    
    InfraxError err = core_mutex->mutex_lock(core_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) return 99;
    
    core = InfraxCoreClass.singleton();
    
    core_mutex->mutex_unlock(core_mutex);

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
    if (test_mutex) InfraxSyncClass.free(test_mutex);
    if (test_cond) InfraxSyncClass.free(test_cond);
    if (core_mutex) InfraxSyncClass.free(core_mutex);
    
    if (core) core->printf(core, "All infrax_net tests passed!\n");
    return 0;
}
