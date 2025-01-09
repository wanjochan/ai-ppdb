#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "peer/peer_server.inc.c"

// Forward declarations
static int run_server(int argc, char* argv[]);

// Main entry point
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [options]\n", argv[0]);
        return 1;
    }
    
    // Parse command
    const char* command = argv[1];
    if (strcmp(command, "server") == 0) {
        return run_server(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Unknown command: %s\n", command);
        return 1;
    }
}

// Run server command
static int run_server(int argc, char* argv[]) {
    // Parse options
    ppdb_options_t options = {0};
    ppdb_net_config_t net_config = {0};
    
    // Set defaults
    options.mode = "memkv";
    options.protocol = "memcached";
    options.threads = 4;
    options.max_connections = 1000;
    options.db_path = "data";
    options.cache_size = 1024 * 1024 * 1024; // 1GB
    options.max_readers = 126;
    options.sync_writes = true;
    options.flush_period_ms = 1000;
    
    net_config.host = "127.0.0.1";
    net_config.port = 11211;
    net_config.timeout_ms = 1000;
    net_config.max_connections = 1000;
    net_config.io_threads = 4;
    net_config.use_tcp_nodelay = true;
    net_config.protocol = "memcached";
    
    // Parse command line options
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            options.mode = argv[++i];
        } else if (strcmp(argv[i], "--protocol") == 0 && i + 1 < argc) {
            options.protocol = argv[++i];
            net_config.protocol = argv[i];
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            options.threads = atoi(argv[++i]);
            net_config.io_threads = options.threads;
        } else if (strcmp(argv[i], "--max-connections") == 0 && i + 1 < argc) {
            options.max_connections = atoi(argv[++i]);
            net_config.max_connections = options.max_connections;
        } else if (strcmp(argv[i], "--db-path") == 0 && i + 1 < argc) {
            options.db_path = argv[++i];
        } else if (strcmp(argv[i], "--cache-size") == 0 && i + 1 < argc) {
            options.cache_size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max-readers") == 0 && i + 1 < argc) {
            options.max_readers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sync-writes") == 0 && i + 1 < argc) {
            options.sync_writes = atoi(argv[++i]) != 0;
        } else if (strcmp(argv[i], "--flush-period") == 0 && i + 1 < argc) {
            options.flush_period_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            net_config.host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            net_config.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            net_config.timeout_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tcp-nodelay") == 0 && i + 1 < argc) {
            net_config.use_tcp_nodelay = atoi(argv[++i]) != 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }
    
    // Create database context
    ppdb_ctx_t* ctx = NULL;
    ppdb_error_t err = ppdb_create(&ctx, &options);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create database context\n");
        return 1;
    }
    
    // Create server
    err = ppdb_server_create(&ctx->server, ctx, &net_config);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create server\n");
        ppdb_destroy(ctx);
        return 1;
    }
    
    // Start server
    err = ppdb_server_start(ctx->server);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to start server\n");
        ppdb_destroy(ctx);
        return 1;
    }
    
    // Wait for signal
    printf("Server running on %s:%d...\n", net_config.host, net_config.port);
    printf("Press Ctrl+C to stop\n");
    
    // TODO: Implement signal handling
    while (1) {
        sleep(1);
    }
    
    // Stop server
    ppdb_server_stop(ctx->server);
    ppdb_destroy(ctx);
    return 0;
} 
