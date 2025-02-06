#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
#include <assert.h>
#include <string.h>

static InfraxSync* test_mutex = NULL;
static InfraxSync* test_cond = NULL;
static bool server_ready = false;

static void test_config() {
    printf("Testing socket configuration...\n");
    
    InfraxSocket* socket = NULL;
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    socket = InfraxSocket_CLASS.new(&config);
    assert(socket != NULL);
    
    // Test UDP configuration
    config.is_udp = true;
    socket = InfraxSocket_CLASS.new(&config);
    assert(socket != NULL);
    
    // Test non-blocking configuration
    config.is_nonblocking = true;
    socket = InfraxSocket_CLASS.new(&config);
    assert(socket != NULL);
    
    printf("Socket configuration tests passed\n");
}

static void* tcp_server_thread(void* arg) {
    (void)arg;
    InfraxSocket* server = NULL;
    InfraxSocket* client = NULL;
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    server = InfraxSocket_CLASS.new(&config);
    assert(server != NULL);
    
    InfraxNetAddr addr;
    strcpy(addr.ip, "127.0.0.1");
    addr.port = 8080;
    
    // Bind to a different port to avoid conflicts
    addr.port = 9090;
    InfraxError err = server->bind(server, &addr);
    assert(INFRAX_ERROR_IS_OK(err));
    
    err = server->listen(server, 5);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Signal that server is ready
    err = test_mutex->mutex_lock(test_mutex);
    assert(INFRAX_ERROR_IS_OK(err));
    server_ready = true;
    err = test_cond->cond_signal(test_cond);
    assert(INFRAX_ERROR_IS_OK(err));
    err = test_mutex->mutex_unlock(test_mutex);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Accept client connection
    err = server->accept(server, &client, NULL);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Echo data back to client
    char buffer[256];
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    assert(INFRAX_ERROR_IS_OK(err));
    
    size_t sent;
    err = client->send(client, buffer, received, &sent);
    assert(INFRAX_ERROR_IS_OK(err));
    assert(sent == received);
    
    InfraxSocket_CLASS.free(client);
    InfraxSocket_CLASS.free(server);
    
    return NULL;
}

static void* udp_server_thread(void* arg) {
    (void)arg;
    InfraxSocket* server = NULL;
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    server = InfraxSocket_CLASS.new(&config);
    assert(server != NULL);
    
    InfraxNetAddr addr;
    strcpy(addr.ip, "127.0.0.1");
    addr.port = 8081;  // Use a different port for UDP
    
    InfraxError err = server->bind(server, &addr);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Signal that server is ready
    err = test_mutex->mutex_lock(test_mutex);
    assert(INFRAX_ERROR_IS_OK(err));
    server_ready = true;
    err = test_cond->cond_signal(test_cond);
    assert(INFRAX_ERROR_IS_OK(err));
    err = test_mutex->mutex_unlock(test_mutex);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Receive and echo data
    char buffer[256];
    size_t received;
    InfraxNetAddr client_addr;
    err = server->recv(server, buffer, sizeof(buffer), &received);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Send back to the client using the peer address stored in the server socket
    size_t sent;
    err = server->send(server, buffer, received, &sent);
    assert(INFRAX_ERROR_IS_OK(err));
    assert(sent == received);
    
    InfraxSocket_CLASS.free(server);
    
    return NULL;
}

static void test_tcp() {
    printf("Testing TCP socket operations...\n");
    
    // Create server thread
    InfraxThreadConfig thread_config = {
        .name = "tcp_server",
        .entry_point = tcp_server_thread,
        .arg = NULL
    };
    InfraxThread* server_thread = InfraxThread_CLASS.new(&thread_config);
    assert(server_thread != NULL);
    
    InfraxError err = server_thread->start(server_thread);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Wait for server to be ready
    err = test_mutex->mutex_lock(test_mutex);
    assert(INFRAX_ERROR_IS_OK(err));
    while (!server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        assert(INFRAX_ERROR_IS_OK(err));
    }
    err = test_mutex->mutex_unlock(test_mutex);
    assert(INFRAX_ERROR_IS_OK(err));
    server_ready = false;
    
    // Create client socket
    InfraxSocket* client = NULL;
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    client = InfraxSocket_CLASS.new(&config);
    assert(client != NULL);
    
    InfraxNetAddr addr;
    strcpy(addr.ip, "127.0.0.1");
    addr.port = 9090;  // Connect to the new server port
    
    err = client->connect(client, &addr);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Send and receive data
    const char* test_data = "Hello, TCP!";
    size_t sent;
    err = client->send(client, test_data, strlen(test_data), &sent);
    assert(INFRAX_ERROR_IS_OK(err));
    assert(sent == strlen(test_data));
    
    char buffer[256];
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    assert(INFRAX_ERROR_IS_OK(err));
    assert(received == sent);
    assert(memcmp(buffer, test_data, received) == 0);
    
    InfraxSocket_CLASS.free(client);
    err = server_thread->join(server_thread, NULL);
    assert(INFRAX_ERROR_IS_OK(err));
    InfraxThread_CLASS.free(server_thread);
    
    printf("TCP socket tests passed\n");
}

static void test_udp() {
    printf("Testing UDP socket operations...\n");
    
    // Create server thread
    InfraxThreadConfig thread_config = {
        .name = "udp_server",
        .entry_point = udp_server_thread,
        .arg = NULL
    };
    InfraxThread* server_thread = InfraxThread_CLASS.new(&thread_config);
    assert(server_thread != NULL);
    
    InfraxError err = server_thread->start(server_thread);
    assert(INFRAX_ERROR_IS_OK(err));
    
    // Wait for server to be ready
    err = test_mutex->mutex_lock(test_mutex);
    assert(INFRAX_ERROR_IS_OK(err));
    while (!server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        assert(INFRAX_ERROR_IS_OK(err));
    }
    err = test_mutex->mutex_unlock(test_mutex);
    assert(INFRAX_ERROR_IS_OK(err));
    server_ready = false;
    
    // Create client socket
    InfraxSocket* client = NULL;
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    client = InfraxSocket_CLASS.new(&config);
    assert(client != NULL);
    
    // Set server address
    InfraxNetAddr addr;
    strcpy(addr.ip, "127.0.0.1");
    addr.port = 8081;  // Use the UDP server port
    client->peer_addr = addr;  // Set the peer address for sending
    
    // Send and receive data
    const char* test_data = "Hello, UDP!";
    size_t sent;
    err = client->send(client, test_data, strlen(test_data), &sent);
    assert(INFRAX_ERROR_IS_OK(err));
    assert(sent == strlen(test_data));
    
    char buffer[256];
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    assert(INFRAX_ERROR_IS_OK(err));
    assert(received == sent);
    assert(memcmp(buffer, test_data, received) == 0);
    
    InfraxSocket_CLASS.free(client);
    err = server_thread->join(server_thread, NULL);
    assert(INFRAX_ERROR_IS_OK(err));
    InfraxThread_CLASS.free(server_thread);
    
    printf("UDP socket tests passed\n");
}

int main() {
    printf("Starting InfraxNet tests...\n");
    
    // Initialize synchronization primitives
    test_mutex = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_MUTEX);
    assert(test_mutex != NULL);
    
    test_cond = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_CONDITION);
    assert(test_cond != NULL);
    
    test_config();
    test_tcp();
    test_udp();
    
    InfraxSync_CLASS.free(test_mutex);
    InfraxSync_CLASS.free(test_cond);
    
    printf("All InfraxNet tests passed!\n");
    return 0;
}
