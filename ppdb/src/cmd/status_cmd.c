#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "cmd_internal.h"

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static void print_usage(void) {
    printf("Usage: ppdb status [options]\n");
    printf("\n");
    printf("Options:\n");
    printf("  --host=<addr>     Server address (default: 127.0.0.1)\n");
    printf("  --port=<port>     Server port (default: 11211)\n");
    printf("  --timeout=<ms>    Operation timeout (default: 1000)\n");
}

//-----------------------------------------------------------------------------
// Command Entry
//-----------------------------------------------------------------------------

ppdb_error_t cmd_status(int argc, char** argv) {
    // Parse options
    const char* host = "127.0.0.1";
    uint16_t port = 11211;
    uint32_t timeout = 1000;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return PPDB_OK;
        } else if (strncmp(argv[i], "--host=", 7) == 0) {
            host = argv[i] + 7;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            port = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--timeout=", 10) == 0) {
            timeout = atoi(argv[i] + 10);
        }
    }

    // Create context
    ppdb_ctx_t ctx;
    ppdb_options_t options = {
        .db_path = NULL,
        .cache_size = 0,
        .max_readers = 1,
        .sync_writes = false,
        .flush_period_ms = 0
    };

    ppdb_error_t err = ppdb_create(&ctx, &options);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create context: %d\n", err);
        return err;
    }

    // Configure client
    ppdb_net_config_t config = {
        .host = host,
        .port = port,
        .timeout_ms = timeout,
        .max_connections = 1,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Connect to server
    ppdb_conn_t conn;
    err = ppdb_client_connect(ctx, &config, &conn);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to connect: %d\n", err);
        ppdb_destroy(ctx);
        return err;
    }

    // Get server stats
    char stats[1024];
    err = ppdb_server_get_stats(ctx, stats, sizeof(stats));
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to get server stats: %d\n", err);
    } else {
        printf("Server Status:\n%s\n", stats);
    }

    // Cleanup
    ppdb_client_disconnect(conn);
    ppdb_destroy(ctx);
    return err;
} 