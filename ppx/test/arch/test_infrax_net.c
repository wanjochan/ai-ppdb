#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"

InfraxCore* core = NULL;

static void test_net_invalid_address(void) {
    core->printf(core, "Testing invalid network address...\n");

    InfraxNetConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };

    InfraxNet* net = InfraxNetClass.new(&config);
    INFRAX_ASSERT(core, net != NULL);

    // 测试无效的 IP 地址
    InfraxNetAddr invalid_addr = {0};
    core->strncpy(core, invalid_addr.ip, "999.999.999.999", sizeof(invalid_addr.ip));
    invalid_addr.port = 12345;

    InfraxError err = net->klass->bind(net, &invalid_addr);
    INFRAX_ASSERT(core, err.code == INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE);
    core->printf(core, "Invalid IP address test passed\n");

    // 测试无效的端口号
    InfraxNetAddr invalid_port_addr = {0};
    core->strncpy(core, invalid_port_addr.ip, "127.0.0.1", sizeof(invalid_port_addr.ip));
    invalid_port_addr.port = 0;  // 端口号 0 是无效的

    err = net->klass->bind(net, &invalid_port_addr);
    INFRAX_ASSERT(core, err.code == INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE);
    core->printf(core, "Invalid port test passed\n");

    InfraxNetClass.free(net);
    core->printf(core, "test_net_invalid_address passed\n");
}

static void test_net_connection_timeout(void) {
    core->printf(core, "Testing network connection timeout...\n");

    InfraxNetConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };

    InfraxNet* net = InfraxNetClass.new(&config);
    INFRAX_ASSERT(core, net != NULL);

    InfraxNetAddr addr = {0};
    core->strncpy(core, addr.ip, "127.0.0.1", sizeof(addr.ip));
    addr.port = 12345;

    InfraxError err = net->klass->connect(net, &addr);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_ERR(err));

    InfraxNetClass.free(net);
    core->printf(core, "test_net_connection_timeout passed\n");
}

static void test_net_tcp_boundary_conditions(void) {
    core->printf(core, "Testing TCP boundary conditions...\n");

    InfraxNetConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };

    InfraxNet* net = InfraxNetClass.new(&config);
    INFRAX_ASSERT(core, net != NULL);

    // Test sending/receiving with NULL buffer
    size_t bytes = 0;
    InfraxError err = net->klass->send(net, NULL, 0, &bytes);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_ERR(err));

    err = net->klass->recv(net, NULL, 0, &bytes);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_ERR(err));

    InfraxNetClass.free(net);
    core->printf(core, "test_net_tcp_boundary_conditions passed\n");
}

static void test_net_udp_boundary_conditions(void) {
    core->printf(core, "Testing UDP boundary conditions...\n");

    InfraxNetConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };

    InfraxNet* udp_net = InfraxNetClass.new(&config);
    INFRAX_ASSERT(core, udp_net != NULL);

    // Test sending to invalid address
    size_t bytes = 0;
    char* data = "test";
    InfraxNetAddr invalid_addr = {0};
    core->strncpy(core, invalid_addr.ip, "999.999.999.999", sizeof(invalid_addr.ip));
    invalid_addr.port = 0;

    InfraxError err = udp_net->klass->sendto(udp_net, data, 4, &bytes, &invalid_addr);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_ERR(err));

    InfraxNetClass.free(udp_net);
    core->printf(core, "test_net_udp_boundary_conditions passed\n");
}

int main(void) {
    core = InfraxCoreClass.singleton();
    INFRAX_ASSERT(core, core != NULL);

    core->printf(core, "Starting InfraxNet tests...\n");
    
    test_net_invalid_address();
    test_net_connection_timeout();
    test_net_tcp_boundary_conditions();
    test_net_udp_boundary_conditions();
    
    core->printf(core, "All InfraxNet tests passed!\n");
    
    // 不需要显式销毁 singleton
    return 0;
}
