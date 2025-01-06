#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "cmd_internal.h"

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static void print_usage(void) {
    printf("Usage: ppdb server <command> [options]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  start     Start the server\n");
    printf("  stop      Stop the server\n");
    printf("  status    Show server status\n");
    printf("\n");
    printf("Options:\n");
    printf("  --host=<addr>     Host address (default: 127.0.0.1)\n");
    printf("  --port=<port>     Port number (default: 11211)\n");
    printf("  --threads=<num>   IO thread count (default: 4)\n");
    printf("  --max-conn=<num>  Max connections (default: 1000)\n");
    printf("  --nodelay         Enable TCP_NODELAY\n");
}

static ppdb_error_t handle_start(int argc, char** argv) {
    // Parse options
    const char* host = "127.0.0.1";
    uint16_t port = 11211;
    uint32_t threads = 4;
    uint32_t max_conn = 1000;
    bool nodelay = false;

    for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "--host=", 7) == 0) {
            host = argv[i] + 7;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            port = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--threads=", 10) == 0) {
            threads = atoi(argv[i] + 10);
        } else if (strncmp(argv[i], "--max-conn=", 11) == 0) {
            max_conn = atoi(argv[i] + 11);
        } else if (strcmp(argv[i], "--nodelay") == 0) {
            nodelay = true;
        }
    }

    // Create database context
    ppdb_ctx_t ctx;
    ppdb_options_t options = {
        .db_path = "data",
        .cache_size = 1024 * 1024 * 1024,  // 1GB
        .max_readers = max_conn,
        .sync_writes = true,
        .flush_period_ms = 1000
    };

    ppdb_error_t err = ppdb_create(&ctx, &options);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create database context: %d\n", err);
        return err;
    }

    // Configure server
    ppdb_net_config_t config = {
        .host = host,
        .port = port,
        .timeout_ms = 30000,
        .max_connections = max_conn,
        .io_threads = threads,
        .use_tcp_nodelay = nodelay
    };

    // Start server
    err = ppdb_server_start(ctx, &config);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to start server: %d\n", err);
        ppdb_destroy(ctx);
        return err;
    }

    printf("Server started on %s:%d\n", host, port);
    return PPDB_OK;
}

static ppdb_error_t handle_stop(int argc, char** argv) {
    // Get server context
    ppdb_ctx_t ctx;
    ppdb_error_t err = ppdb_get_context(&ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Server not running\n");
        return err;
    }

    // Stop server
    err = ppdb_server_stop(ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to stop server: %d\n", err);
        return err;
    }

    printf("Server stopped\n");
    return PPDB_OK;
}

static ppdb_error_t handle_status(int argc, char** argv) {
    // Get server context
    ppdb_ctx_t ctx;
    ppdb_error_t err = ppdb_get_context(&ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Server not running\n");
        return err;
    }

    // Get stats
    char stats[1024];
    err = ppdb_server_get_stats(ctx, stats, sizeof(stats));
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to get server stats: %d\n", err);
        return err;
    }

    printf("Server Status:\n%s\n", stats);
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Command Entry
//-----------------------------------------------------------------------------

ppdb_error_t cmd_server(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return PPDB_ERR_PARAM;
    }

    const char* cmd = argv[1];
    if (strcmp(cmd, "start") == 0) {
        return handle_start(argc, argv);
    } else if (strcmp(cmd, "stop") == 0) {
        return handle_stop(argc, argv);
    } else if (strcmp(cmd, "status") == 0) {
        return handle_status(argc, argv);
    } else {
        print_usage();
        return PPDB_ERR_PARAM;
    }
}