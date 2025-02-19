#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxMemory.h"

InfraxCore* core = NULL;
InfraxAsync* async = NULL;

// 用于异步TCP测试的上下文结构
typedef struct {
    InfraxNet* server;
    InfraxNet* client;
    bool connected;
    char buffer[1024];
    size_t bytes;
} AsyncTcpContext;

// 用于异步UDP测试的上下文结构
typedef struct {
    InfraxNet* socket;
    InfraxNetAddr peer_addr;
    char buffer[1024];
    size_t bytes;
} AsyncUdpContext;

// TCP服务器接受连接的回调函数
static void on_tcp_accept(InfraxAsync* self, int fd, short events, void* arg) {
    AsyncTcpContext* ctx = (AsyncTcpContext*)arg;
    InfraxNetAddr client_addr;
    InfraxError err = ctx->server->klass->accept(ctx->server, &ctx->client, &client_addr);
    
    if (INFRAX_ERROR_IS_ERR(err)) {
        if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
            // 继续等待连接，使用 setTimeout 重新调度
            InfraxAsyncClass.setTimeout(10, on_tcp_accept, ctx);
            return;
        }
        core->printf(core, "Accept failed: %s\n", err.message);
        return;
    }
    
    ctx->connected = true;
    core->printf(core, "Client connected from %s:%d\n", client_addr.ip, client_addr.port);
}

// TCP客户端连接回调函数
static void on_tcp_connect(InfraxAsync* self, int fd, short events, void* arg) {
    AsyncTcpContext* ctx = (AsyncTcpContext*)arg;
    const char* test_data = "Hello, Async TCP!";
    
    InfraxError err = ctx->client->klass->send(ctx->client, test_data, strlen(test_data), &ctx->bytes);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Send failed: %s\n", err.message);
        return;
    }
    
    core->printf(core, "Sent %zu bytes\n", ctx->bytes);
}

// UDP数据发送回调函数
static void on_udp_send(InfraxAsync* self, int fd, short events, void* arg) {
    AsyncUdpContext* ctx = (AsyncUdpContext*)arg;
    const char* test_data = "Hello, Async UDP!";
    
    InfraxError err = ctx->socket->klass->sendto(ctx->socket, test_data, strlen(test_data), 
                                                &ctx->bytes, &ctx->peer_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "UDP send failed: %s\n", err.message);
        return;
    }
    
    core->printf(core, "Sent %zu bytes via UDP\n", ctx->bytes);
}

static void test_async_tcp(void) {
    core->printf(core, "Testing async TCP...\n");
    
    // 创建TCP服务器配置
    InfraxNetConfig server_config = {
        .is_udp = false,
        .is_nonblocking = true,  // 异步模式必须是非阻塞的
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = true
    };
    
    // 创建TCP客户端配置
    InfraxNetConfig client_config = {
        .is_udp = false,
        .is_nonblocking = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = false
    };
    
    AsyncTcpContext ctx = {0};
    ctx.server = InfraxNetClass.new(&server_config);
    ctx.client = InfraxNetClass.new(&client_config);
    INFRAX_ASSERT(core, ctx.server != NULL && ctx.client != NULL);
    
    // 绑定服务器地址
    InfraxNetAddr server_addr = {0};
    core->strncpy(core, server_addr.ip, "127.0.0.1", sizeof(server_addr.ip));
    server_addr.port = 12345;
    
    InfraxError err = ctx.server->klass->bind(ctx.server, &server_addr);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    
    err = ctx.server->klass->listen(ctx.server, 5);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    
    // 启动异步接受连接
    InfraxAsyncClass.setTimeout(10, on_tcp_accept, &ctx);
    
    // 客户端异步连接
    err = ctx.client->klass->connect(ctx.client, &server_addr);
    if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
        InfraxAsyncClass.setTimeout(10, on_tcp_connect, &ctx);
    } else {
        INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    }
    
    // 等待异步操作完成
    core->sleep_ms(core, 2000);
    
    // 清理资源
    InfraxNetClass.free(ctx.server);
    InfraxNetClass.free(ctx.client);
    
    core->printf(core, "Async TCP test completed\n");
}

static void test_async_udp(void) {
    core->printf(core, "Testing async UDP...\n");
    
    InfraxNetConfig config = {
        .is_udp = true,
        .is_nonblocking = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = true
    };
    
    AsyncUdpContext ctx = {0};
    ctx.socket = InfraxNetClass.new(&config);
    INFRAX_ASSERT(core, ctx.socket != NULL);
    
    // 设置UDP目标地址
    core->strncpy(core, ctx.peer_addr.ip, "127.0.0.1", sizeof(ctx.peer_addr.ip));
    ctx.peer_addr.port = 12346;
    
    // 绑定本地地址
    InfraxNetAddr local_addr = {0};
    core->strncpy(core, local_addr.ip, "127.0.0.1", sizeof(local_addr.ip));
    local_addr.port = 12347;
    
    InfraxError err = ctx.socket->klass->bind(ctx.socket, &local_addr);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    
    // 启动异步发送
    InfraxAsyncClass.setTimeout(10, on_udp_send, &ctx);
    
    // 等待异步操作完成
    core->sleep_ms(core, 1000);
    
    // 清理资源
    InfraxNetClass.free(ctx.socket);
    
    core->printf(core, "Async UDP test completed\n");
}

int main(void) {
    core = InfraxCoreClass.singleton();
    INFRAX_ASSERT(core, core != NULL);
    
    // 创建异步执行器，不需要配置结构体
    async = InfraxAsyncClass.new(NULL, NULL);
    INFRAX_ASSERT(core, async != NULL);
    
    core->printf(core, "Starting InfraxNet async tests...\n");
    
    test_async_tcp();
    test_async_udp();
    
    // 清理资源
    InfraxAsyncClass.free(async);
    
    core->printf(core, "All InfraxNet async tests passed!\n");
    return 0;
}