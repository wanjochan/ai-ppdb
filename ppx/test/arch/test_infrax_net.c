#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Forward declarations
static InfraxMemory* get_memory_manager(void) {
    static InfraxMemory* memory = NULL;
    if (!memory) {
        InfraxMemoryConfig config = {
            .initial_size = 1024 * 1024,  // 1MB
            .use_gc = false,
            .use_pool = true,
            .gc_threshold = 0
        };
        memory = InfraxMemoryClass.new(&config);
    }
    return memory;
}

static void test_net_invalid_address(void) {
    InfraxNetConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };

    InfraxNet* net = InfraxNetClass.new(&config);
    assert(net != NULL);

    // 测试无效的 IP 地址
    InfraxNetAddr invalid_addr = {0};
    strncpy(invalid_addr.ip, "999.999.999.999", sizeof(invalid_addr.ip) - 1);
    invalid_addr.port = 12345;

    InfraxError err = net->klass->bind(net, &invalid_addr);
    assert(err.code == INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE);
    printf("Invalid IP address test passed\n");

    // 测试无效的端口号
    InfraxNetAddr invalid_port_addr = {0};
    strncpy(invalid_port_addr.ip, "127.0.0.1", sizeof(invalid_port_addr.ip) - 1);
    invalid_port_addr.port = 0;  // 端口号 0 是无效的

    err = net->klass->bind(net, &invalid_port_addr);
    assert(err.code == INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE);
    printf("Invalid port test passed\n");

    InfraxNetClass.free(net);
    printf("test_net_invalid_address passed\n");
}

static void test_net_connection_timeout(void) {
    InfraxNetConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };

    InfraxNet* net = InfraxNetClass.new(&config);
    assert(net != NULL);

    InfraxNetAddr addr = {0};
    strncpy(addr.ip, "127.0.0.1", sizeof(addr.ip) - 1);
    addr.port = 12345;

    InfraxError err = net->klass->connect(net, &addr);
    assert(INFRAX_ERROR_IS_ERR(err));

    InfraxNetClass.free(net);
    printf("test_net_connection_timeout passed\n");
}

static void test_net_tcp_boundary_conditions(void) {
    InfraxNetConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };

    InfraxNet* net = InfraxNetClass.new(&config);
    assert(net != NULL);

    // Test sending/receiving with NULL buffer
    size_t bytes = 0;
    InfraxError err = net->klass->send(net, NULL, 0, &bytes);
    assert(INFRAX_ERROR_IS_ERR(err));

    err = net->klass->recv(net, NULL, 0, &bytes);
    assert(INFRAX_ERROR_IS_ERR(err));

    InfraxNetClass.free(net);
    printf("test_net_tcp_boundary_conditions passed\n");
}

static void test_net_udp_boundary_conditions(void) {
    InfraxNetConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };

    InfraxNet* udp_net = InfraxNetClass.new(&config);
    assert(udp_net != NULL);

    // Test sending to invalid address
    size_t bytes = 0;
    const char* data = "test";
    InfraxNetAddr invalid_addr = {0};
    strncpy(invalid_addr.ip, "999.999.999.999", sizeof(invalid_addr.ip) - 1);
    invalid_addr.port = 0;

    InfraxError err = udp_net->klass->sendto(udp_net, data, strlen(data), &bytes, &invalid_addr);
    assert(INFRAX_ERROR_IS_ERR(err));

    InfraxNetClass.free(udp_net);
    printf("test_net_udp_boundary_conditions passed\n");
}

int main(void) {
    printf("Starting InfraxNet tests...\n");
    
    test_net_invalid_address();
    test_net_connection_timeout();
    test_net_tcp_boundary_conditions();
    test_net_udp_boundary_conditions();
    
    printf("All InfraxNet tests passed!\n");
    return 0;
}
