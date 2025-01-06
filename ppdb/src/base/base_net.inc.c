//-----------------------------------------------------------------------------
// 内部结构
//-----------------------------------------------------------------------------

// 连接结构
typedef struct ppdb_connection_s {
    int fd;                         // 套接字
    struct ppdb_net_server_s* server; // 所属服务器
    void* proto;                    // 协议处理器
    uint8_t* recv_buffer;          // 接收缓冲区
    size_t recv_size;              // 缓冲区大小
    size_t recv_pos;               // 当前位置
    bool is_closed;                // 是否已关闭
} ppdb_connection_t;

// 服务器结构
typedef struct ppdb_net_server_s {
    int listen_fd;                 // 监听套接字
    const ppdb_net_config_t* config; // 配置信息
    const ppdb_protocol_ops_t* proto_ops; // 协议操作
    void* proto_data;              // 协议数据
    ppdb_connection_t** conns;     // 连接数组
    size_t max_conns;              // 最大连接数
    size_t curr_conns;             // 当前连接数
    bool is_running;               // 是否运行中
    pthread_t* io_threads;         // IO线程数组
    size_t io_thread_count;        // IO线程数量
} ppdb_net_server_t;

//-----------------------------------------------------------------------------
// 连接管理
//-----------------------------------------------------------------------------

static ppdb_connection_t* create_connection(ppdb_net_server_t* server, int fd) {
    ppdb_connection_t* conn = calloc(1, sizeof(ppdb_connection_t));
    if (!conn) {
        return NULL;
    }
    
    conn->fd = fd;
    conn->server = server;
    conn->recv_buffer = malloc(4096);  // 初始4K缓冲区
    if (!conn->recv_buffer) {
        free(conn);
        return NULL;
    }
    conn->recv_size = 4096;
    
    // 创建协议处理器
    if (server->proto_ops->create(&conn->proto, server->proto_data) != PPDB_OK) {
        free(conn->recv_buffer);
        free(conn);
        return NULL;
    }
    
    // 设置非阻塞
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    return conn;
}

static void destroy_connection(ppdb_connection_t* conn) {
    if (!conn) {
        return;
    }
    
    if (conn->proto) {
        conn->server->proto_ops->destroy(conn->proto);
    }
    
    if (conn->fd >= 0) {
        close(conn->fd);
    }
    
    free(conn->recv_buffer);
    free(conn);
}

//-----------------------------------------------------------------------------
// IO处理
//-----------------------------------------------------------------------------

static ppdb_error_t handle_read(ppdb_connection_t* conn) {
    while (1) {
        // 确保缓冲区足够大
        if (conn->recv_pos >= conn->recv_size) {
            size_t new_size = conn->recv_size * 2;
            uint8_t* new_buf = realloc(conn->recv_buffer, new_size);
            if (!new_buf) {
                return PPDB_ERR_MEMORY;
            }
            conn->recv_buffer = new_buf;
            conn->recv_size = new_size;
        }
        
        // 读取数据
        ssize_t n = read(conn->fd, 
                        conn->recv_buffer + conn->recv_pos,
                        conn->recv_size - conn->recv_pos);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return PPDB_OK;
            }
            return PPDB_ERR_IO;
        }
        
        if (n == 0) {
            return PPDB_ERR_IO;  // 连接关闭
        }
        
        conn->recv_pos += n;
        
        // 处理数据
        ppdb_error_t err = conn->server->proto_ops->on_data(
            conn->proto, (ppdb_conn_t)conn,
            conn->recv_buffer, conn->recv_pos);
        
        if (err != PPDB_OK) {
            return err;
        }
        
        // 移动未处理的数据到缓冲区开始
        memmove(conn->recv_buffer, 
                conn->recv_buffer + conn->recv_pos,
                conn->recv_size - conn->recv_pos);
        conn->recv_pos = 0;
    }
}

static void* io_thread_func(void* arg) {
    ppdb_net_server_t* server = (ppdb_net_server_t*)arg;
    
    while (server->is_running) {
        // TODO: 使用epoll/kqueue实现事件循环
        usleep(1000);  // 临时方案
    }
    
    return NULL;
}

//-----------------------------------------------------------------------------
// 服务器接口实现
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_net_server_create(ppdb_net_server_t** server,
                                        const ppdb_net_config_t* config,
                                        const ppdb_protocol_ops_t* proto_ops,
                                        void* proto_data) {
    if (!server || !config || !proto_ops) {
        return PPDB_ERR_PARAM;
    }
    
    ppdb_net_server_t* s = calloc(1, sizeof(ppdb_net_server_t));
    if (!s) {
        return PPDB_ERR_MEMORY;
    }
    
    s->config = config;
    s->proto_ops = proto_ops;
    s->proto_data = proto_data;
    s->max_conns = config->max_connections;
    s->listen_fd = -1;
    
    // 分配连接数组
    s->conns = calloc(config->max_connections, sizeof(ppdb_connection_t*));
    if (!s->conns) {
        free(s);
        return PPDB_ERR_MEMORY;
    }
    
    // 分配IO线程数组
    s->io_threads = calloc(config->io_threads, sizeof(pthread_t));
    if (!s->io_threads) {
        free(s->conns);
        free(s);
        return PPDB_ERR_MEMORY;
    }
    s->io_thread_count = config->io_threads;
    
    *server = s;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_net_server_start(ppdb_net_server_t* server) {
    if (!server) {
        return PPDB_ERR_PARAM;
    }
    
    // 创建监听套接字
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        return PPDB_ERR_NETWORK;
    }
    
    // 设置地址重用
    int reuse = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->config->port);
    addr.sin_addr.s_addr = inet_addr(server->config->host);
    
    if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        return PPDB_ERR_NETWORK;
    }
    
    // 开始监听
    if (listen(server->listen_fd, 1024) < 0) {
        close(server->listen_fd);
        return PPDB_ERR_NETWORK;
    }
    
    // 启动IO线程
    server->is_running = true;
    for (size_t i = 0; i < server->io_thread_count; i++) {
        if (pthread_create(&server->io_threads[i], NULL, io_thread_func, server) != 0) {
            server->is_running = false;
            close(server->listen_fd);
            return PPDB_ERR_NETWORK;
        }
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_net_server_stop(ppdb_net_server_t* server) {
    if (!server) {
        return PPDB_ERR_PARAM;
    }
    
    // 停止IO线程
    server->is_running = false;
    for (size_t i = 0; i < server->io_thread_count; i++) {
        pthread_join(server->io_threads[i], NULL);
    }
    
    // 关闭所有连接
    for (size_t i = 0; i < server->max_conns; i++) {
        if (server->conns[i]) {
            destroy_connection(server->conns[i]);
            server->conns[i] = NULL;
        }
    }
    
    // 关闭监听套接字
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }
    
    return PPDB_OK;
}

void ppdb_base_net_server_destroy(ppdb_net_server_t* server) {
    if (!server) {
        return;
    }
    
    ppdb_base_net_server_stop(server);
    free(server->io_threads);
    free(server->conns);
    free(server);
}

ppdb_error_t ppdb_base_net_server_get_stats(ppdb_net_server_t* server,
                                           char* buffer, size_t size) {
    if (!server || !buffer || size == 0) {
        return PPDB_ERR_PARAM;
    }
    
    int len = snprintf(buffer, size,
                      "Server Stats:\n"
                      "  Connections: %zu/%zu\n"
                      "  IO Threads: %zu\n",
                      server->curr_conns,
                      server->max_conns,
                      server->io_thread_count);
    
    if (len >= size) {
        return PPDB_ERR_BUFFER_TOO_SMALL;
    }
    
    return PPDB_OK;
} 