#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxMemory.h"

InfraxCore* core = NULL;
InfraxAsync* async = NULL;
InfraxMemory* memory = NULL;

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

// 用于并发TCP测试的上下文结构
typedef struct {
    InfraxNet* server;
    InfraxNet** clients;  // 客户端数组
    int client_count;     // 客户端总数
    int connected_count;  // 已连接客户端数
    bool* client_connected; // 客户端连接状态
    bool* client_sent;    // 客户端发送状态
    char buffer[1024];
    size_t bytes;
} ConcurrentTcpContext;

// 用于并发UDP测试的上下文结构
typedef struct {
    InfraxNet** sockets;  // UDP socket数组
    int socket_count;     // socket总数
    int sent_count;       // 已发送数据的socket数
    bool* socket_sent;    // socket发送状态
    InfraxNetAddr* peer_addrs; // 目标地址数组
    char buffer[1024];
    size_t bytes;
} ConcurrentUdpContext;

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

// 并发TCP服务器接受连接的回调函数
static void on_concurrent_tcp_accept(InfraxAsync* self, int fd, short events, void* arg) {
    ConcurrentTcpContext* ctx = (ConcurrentTcpContext*)arg;
    InfraxNetAddr client_addr;
    
    // 尝试接受新连接
    InfraxNet* new_client = NULL;
    InfraxError err = ctx->server->klass->accept(ctx->server, &new_client, &client_addr);
    
    if (INFRAX_ERROR_IS_ERR(err)) {
        if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
            // 继续等待连接
            InfraxAsyncClass.setTimeout(10, on_concurrent_tcp_accept, ctx);
            return;
        }
        core->printf(core, "Accept failed: %s\n", err.message);
        return;
    }
    
    // 找到一个空闲的客户端槽位
    for (int i = 0; i < ctx->client_count; i++) {
        if (!ctx->client_connected[i]) {
            ctx->clients[i] = new_client;
            ctx->client_connected[i] = true;
            ctx->connected_count++;
            core->printf(core, "Client %d connected from %s:%d\n", i, client_addr.ip, client_addr.port);
            break;
        }
    }
    
    // 继续等待更多连接
    if (ctx->connected_count < ctx->client_count) {
        InfraxAsyncClass.setTimeout(10, on_concurrent_tcp_accept, ctx);
    }
}

// 并发TCP客户端连接回调函数
static void on_concurrent_tcp_connect(InfraxAsync* self, int fd, short events, void* arg) {
    ConcurrentTcpContext* ctx = (ConcurrentTcpContext*)arg;
    const char* test_data = "Hello from concurrent client!";
    
    // 为所有未发送数据的已连接客户端发送数据
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->client_connected[i] && !ctx->client_sent[i]) {
            InfraxError err = ctx->clients[i]->klass->send(ctx->clients[i], test_data, strlen(test_data), &ctx->bytes);
            if (INFRAX_ERROR_IS_ERR(err)) {
                if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                    continue;
                }
                core->printf(core, "Client %d send failed: %s\n", i, err.message);
                continue;
            }
            
            ctx->client_sent[i] = true;
            core->printf(core, "Client %d sent %zu bytes\n", i, ctx->bytes);
        }
    }
    
    // 检查是否所有客户端都已发送数据
    bool all_sent = true;
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->client_connected[i] && !ctx->client_sent[i]) {
            all_sent = false;
            break;
        }
    }
    
    // 如果还有客户端未发送数据，继续尝试
    if (!all_sent) {
        InfraxAsyncClass.setTimeout(10, on_concurrent_tcp_connect, ctx);
    }
}

// 并发UDP发送回调函数
static void on_concurrent_udp_send(InfraxAsync* self, int fd, short events, void* arg) {
    ConcurrentUdpContext* ctx = (ConcurrentUdpContext*)arg;
    const char* test_data = "Hello from concurrent UDP!";
    
    // 为所有未发送数据的socket发送数据
    for (int i = 0; i < ctx->socket_count; i++) {
        if (!ctx->socket_sent[i]) {
            InfraxError err = ctx->sockets[i]->klass->sendto(ctx->sockets[i], test_data, strlen(test_data),
                                                           &ctx->bytes, &ctx->peer_addrs[i]);
            if (INFRAX_ERROR_IS_ERR(err)) {
                if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                    continue;
                }
                core->printf(core, "UDP socket %d send failed: %s\n", i, err.message);
                continue;
            }
            
            ctx->socket_sent[i] = true;
            ctx->sent_count++;
            core->printf(core, "UDP socket %d sent %zu bytes\n", i, ctx->bytes);
        }
    }
    
    // 如果还有socket未发送数据，继续尝试
    if (ctx->sent_count < ctx->socket_count) {
        InfraxAsyncClass.setTimeout(10, on_concurrent_udp_send, ctx);
    }
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

// 并发TCP测试
static void test_concurrent_tcp(int num_clients) {
    core->printf(core, "Testing concurrent TCP with %d clients...\n", num_clients);
    
    // 创建TCP服务器配置
    InfraxNetConfig server_config = {
        .is_udp = false,
        .is_nonblocking = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_addr = true
    };
    
    // 创建并初始化上下文
    ConcurrentTcpContext ctx = {0};
    ctx.client_count = num_clients;
    ctx.clients = memory->alloc(memory, sizeof(InfraxNet*) * num_clients);
    ctx.client_connected = memory->alloc(memory, sizeof(bool) * num_clients);
    ctx.client_sent = memory->alloc(memory, sizeof(bool) * num_clients);
    INFRAX_ASSERT(core, ctx.clients != NULL && ctx.client_connected != NULL && ctx.client_sent != NULL);
    
    // 初始化服务器
    ctx.server = InfraxNetClass.new(&server_config);
    INFRAX_ASSERT(core, ctx.server != NULL);
    
    // 绑定服务器地址
    InfraxNetAddr server_addr = {0};
    core->strncpy(core, server_addr.ip, "127.0.0.1", sizeof(server_addr.ip));
    server_addr.port = 12345;
    
    InfraxError err = ctx.server->klass->bind(ctx.server, &server_addr);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    
    err = ctx.server->klass->listen(ctx.server, num_clients);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    
    // 启动异步接受连接
    InfraxAsyncClass.setTimeout(10, on_concurrent_tcp_accept, &ctx);
    
    // 创建并连接客户端
    for (int i = 0; i < num_clients; i++) {
        InfraxNetConfig client_config = {
            .is_udp = false,
            .is_nonblocking = true,
            .send_timeout_ms = 1000,
            .recv_timeout_ms = 1000,
            .reuse_addr = false
        };
        
        ctx.clients[i] = InfraxNetClass.new(&client_config);
        INFRAX_ASSERT(core, ctx.clients[i] != NULL);
        
        err = ctx.clients[i]->klass->connect(ctx.clients[i], &server_addr);
        if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
            InfraxAsyncClass.setTimeout(10, on_concurrent_tcp_connect, &ctx);
        } else {
            INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
            ctx.client_connected[i] = true;
            ctx.connected_count++;
        }
    }
    
    // 等待所有操作完成
    core->sleep_ms(core, 5000);
    
    // 清理资源
    for (int i = 0; i < num_clients; i++) {
        if (ctx.clients[i] != NULL) {
            InfraxNetClass.free(ctx.clients[i]);
        }
    }
    InfraxNetClass.free(ctx.server);
    memory->dealloc(memory, ctx.clients);
    memory->dealloc(memory, ctx.client_connected);
    memory->dealloc(memory, ctx.client_sent);
    
    core->printf(core, "Concurrent TCP test completed\n");
}

// 并发UDP测试
static void test_concurrent_udp(int num_sockets) {
    core->printf(core, "Testing concurrent UDP with %d sockets...\n", num_sockets);
    
    // 创建并初始化上下文
    ConcurrentUdpContext ctx = {0};
    ctx.socket_count = num_sockets;
    ctx.sockets = memory->alloc(memory, sizeof(InfraxNet*) * num_sockets);
    ctx.socket_sent = memory->alloc(memory, sizeof(bool) * num_sockets);
    ctx.peer_addrs = memory->alloc(memory, sizeof(InfraxNetAddr) * num_sockets);
    INFRAX_ASSERT(core, ctx.sockets != NULL && ctx.socket_sent != NULL && ctx.peer_addrs != NULL);
    
    // 创建UDP sockets
    for (int i = 0; i < num_sockets; i++) {
        InfraxNetConfig config = {
            .is_udp = true,
            .is_nonblocking = true,
            .send_timeout_ms = 1000,
            .recv_timeout_ms = 1000,
            .reuse_addr = true
        };
        
        ctx.sockets[i] = InfraxNetClass.new(&config);
        INFRAX_ASSERT(core, ctx.sockets[i] != NULL);
        
        // 绑定本地地址
        InfraxNetAddr local_addr = {0};
        core->strncpy(core, local_addr.ip, "127.0.0.1", sizeof(local_addr.ip));
        local_addr.port = 12347 + i;  // 每个socket使用不同的端口
        
        InfraxError err = ctx.sockets[i]->klass->bind(ctx.sockets[i], &local_addr);
        INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
        
        // 设置目标地址
        core->strncpy(core, ctx.peer_addrs[i].ip, "127.0.0.1", sizeof(ctx.peer_addrs[i].ip));
        ctx.peer_addrs[i].port = 12346;  // 所有socket发送到同一个目标端口
    }
    
    // 启动异步发送
    InfraxAsyncClass.setTimeout(10, on_concurrent_udp_send, &ctx);
    
    // 等待所有操作完成
    core->sleep_ms(core, 3000);
    
    // 清理资源
    for (int i = 0; i < num_sockets; i++) {
        if (ctx.sockets[i] != NULL) {
            InfraxNetClass.free(ctx.sockets[i]);
        }
    }
    memory->dealloc(memory, ctx.sockets);
    memory->dealloc(memory, ctx.socket_sent);
    memory->dealloc(memory, ctx.peer_addrs);
    
    core->printf(core, "Concurrent UDP test completed\n");
}

int main(void) {
    core = InfraxCoreClass.singleton();
    INFRAX_ASSERT(core, core != NULL);
    
    // 创建内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    memory = InfraxMemoryClass.new(&mem_config);
    INFRAX_ASSERT(core, memory != NULL);
    
    // 创建异步执行器
    async = InfraxAsyncClass.new(NULL, NULL);
    INFRAX_ASSERT(core, async != NULL);
    
    core->printf(core, "Starting InfraxNet async tests...\n");
    
    // 运行基本测试
    test_async_tcp();
    test_async_udp();
    
    // 运行并发测试
    test_concurrent_tcp(10);  // 10个并发TCP客户端
    test_concurrent_udp(10);  // 10个并发UDP socket
    
    // 清理资源
    InfraxAsyncClass.free(async);
    InfraxMemoryClass.free(memory);
    
    core->printf(core, "All InfraxNet async tests passed!\n");
    return 0;
}