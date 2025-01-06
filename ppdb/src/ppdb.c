#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "internal/peer.h"

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static ppdb_error_t cmd_server(int argc, char** argv);
static ppdb_error_t cmd_client(int argc, char** argv);
static ppdb_error_t cmd_status(int argc, char** argv);

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

static bool g_initialized = false;
static volatile sig_atomic_t g_running = 1;

//-----------------------------------------------------------------------------
// Signal Handling
//-----------------------------------------------------------------------------

static void OnSignal(int sig) {
    write(1, "\nShutting down...\n", 17);
    g_running = 0;
}

static void SetupSignalHandlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = OnSignal;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    
    // 注册多个信号
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_init(void) {
    if (g_initialized) {
        return PPDB_OK;
    }
    
    // TODO: Add initialization logic
    g_initialized = true;
    return PPDB_OK;
}

ppdb_error_t ppdb_cleanup(void) {
    if (!g_initialized) {
        return PPDB_OK;
    }
    
    // TODO: Add cleanup logic
    g_initialized = false;
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Command Line Help
//-----------------------------------------------------------------------------

static void print_usage(void) {
    printf("Usage: ppdb <command> [options]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  server    Server management commands\n");
    printf("  client    Client operations\n");
    printf("  status    Show status information\n");
    printf("  help      Show this help message\n");
    printf("\n");
    printf("For command-specific help, run:\n");
    printf("  ppdb <command> --help\n");
}

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static void print_server_usage(void) {
    printf("Usage: ppdb server [options]\n");
    printf("\n");
    printf("Options:\n");
    printf("  --mode=<type>      Server mode (default: memkv)\n");
    printf("  --protocol=<type>  Protocol type (default: memcached)\n");
    printf("                     Supported: memcached, redis, binary\n");
    printf("  --host=<addr>      Host address (default: 127.0.0.1)\n");
    printf("  --port=<port>      Port number (default: 11211)\n");
    printf("  --threads=<num>    IO thread count (default: 4)\n");
    printf("  --max-conn=<num>   Max connections (default: 1000)\n");
    printf("  --help            Show this help message\n");
}

static ppdb_error_t cmd_server(int argc, char** argv) {
    // 默认配置
    const char* mode = "memkv";
    const char* protocol = "memcached";
    const char* host = "127.0.0.1";
    uint16_t port = 11211;
    uint32_t threads = 4;
    uint32_t max_conn = 1000;
    
    // 解析参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_server_usage();
            return PPDB_OK;
        }
        
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            mode = argv[i] + 7;
        } else if (strncmp(argv[i], "--protocol=", 11) == 0) {
            protocol = argv[i] + 11;
            // 根据协议自动设置默认端口
            if (strcmp(protocol, "redis") == 0) {
                port = 6379;
            } else if (strcmp(protocol, "memcached") == 0) {
                port = 11211;
            }
        } else if (strncmp(argv[i], "--host=", 7) == 0) {
            host = argv[i] + 7;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            port = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--threads=", 10) == 0) {
            threads = atoi(argv[i] + 10);
        } else if (strncmp(argv[i], "--max-conn=", 11) == 0) {
            max_conn = atoi(argv[i] + 11);
        } else {
            fprintf(stderr, "Unknown option: %s\n\n", argv[i]);
            print_server_usage();
            return PPDB_ERR_PARAM;
        }
    }
    
    // 验证mode类型
    if (strcmp(mode, "memkv") != 0) {
        fprintf(stderr, "Unsupported mode: %s\n", mode);
        return PPDB_ERR_PARAM;
    }
    
    // 验证protocol类型
    if (strcmp(protocol, "memcached") != 0 && 
        strcmp(protocol, "redis") != 0 && 
        strcmp(protocol, "binary") != 0) {
        fprintf(stderr, "Unsupported protocol: %s\n", protocol);
        return PPDB_ERR_PARAM;
    }
    
    // 创建数据库上下文
    ppdb_ctx_t ctx;
    ppdb_options_t options = {
        .db_path = "./tmp",  // 使用临时目录
        .cache_size = 1024 * 1024 * 1024,  // 1GB缓存
        .max_readers = max_conn,
        .sync_writes = false,  // memkv模式下不需要同步写入
        .flush_period_ms = 0,  // memkv模式下不需要定期刷新
        .mode = mode  // 需要在ppdb.h中添加此字段
    };
    
    ppdb_error_t err = ppdb_create(&ctx, &options);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create database context: %d\n", err);
        return err;
    }
    
    // 配置服务器
    ppdb_net_config_t config = {
        .host = host,
        .port = port,
        .timeout_ms = 30000,
        .max_connections = max_conn,
        .io_threads = threads,
        .use_tcp_nodelay = true,
        .protocol = protocol
    };
    
    // 创建并启动服务器
    ppdb_server_t server = NULL;
    err = ppdb_server_create(&server, ctx, &config);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create server: %d\n", err);
        ppdb_destroy(ctx);
        return err;
    }
    
    // 设置信号处理
    g_running = 1;
    SetupSignalHandlers();
    
    // 启动服务器
    printf("Starting %s server with %s protocol on %s:%d...\n", 
           mode, protocol, host, port);
    err = ppdb_server_start(server);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to start server: %d\n", err);
        ppdb_server_destroy(server);
        ppdb_destroy(ctx);
        return err;
    }
    
    // 等待中断信号
    printf("Server is running. Press Ctrl+C to stop.\n");
    fflush(stdout);
    
    while (g_running) {
        if (write(1, ".", 1) == 1) {  // 每秒打印一个点表示程序还在运行
            fflush(stdout);
        }
        Sleep(1000);
    }
    
    printf("\n");  // 换行
    
    // 停止服务器
    printf("Stopping server...\n");
    err = ppdb_server_stop(server);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to stop server: %d\n", err);
    }
    
    ppdb_server_destroy(server);
    ppdb_destroy(ctx);
    return err;
}

static ppdb_error_t cmd_client(int argc, char** argv) {
    PPDB_UNUSED(argc);
    PPDB_UNUSED(argv);
    // TODO: Implement client command
    return PPDB_OK;
}

static ppdb_error_t cmd_status(int argc, char** argv) {
    PPDB_UNUSED(argc);
    PPDB_UNUSED(argv);
    // TODO: Implement status command
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Main Entry
//-----------------------------------------------------------------------------

int main(int argc, char** argv) {
    // Initialize
    ppdb_error_t err = ppdb_init();
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to initialize: %d\n", err);
        return 1;
    }

    // Parse command
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* cmd = argv[1];
    if (strcmp(cmd, "server") == 0) {
        err = cmd_server(argc - 1, argv + 1);
    } else if (strcmp(cmd, "client") == 0) {
        err = cmd_client(argc - 1, argv + 1);
    } else if (strcmp(cmd, "status") == 0) {
        err = cmd_status(argc - 1, argv + 1);
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage();
        err = PPDB_OK;
    } else {
        fprintf(stderr, "Unknown command: %s\n\n", cmd);
        print_usage();
        err = PPDB_ERR_PARAM;
    }

    // Cleanup
    ppdb_cleanup();

    return (err == PPDB_OK) ? 0 : 1;
} 
