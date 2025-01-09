#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "cmd_internal.h"

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static void print_usage(void) {
    printf("Usage: ppdb client <command> [options]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  get <key>              Get value by key\n");
    printf("  put <key> <value>      Put key-value pair\n");
    printf("  delete <key>           Delete key-value pair\n");
    printf("  stats                  Show client statistics\n");
    printf("\n");
    printf("Options:\n");
    printf("  --host=<addr>     Server address (default: 127.0.0.1)\n");
    printf("  --port=<port>     Server port (default: 11211)\n");
    printf("  --timeout=<ms>    Operation timeout (default: 1000)\n");
    printf("  --nodelay         Enable TCP_NODELAY\n");
}

static void on_operation_complete(ppdb_error_t error,
                                void* result,
                                void* user_data) {
    if (error != PPDB_OK) {
        fprintf(stderr, "Operation failed: %d\n", error);
        return;
    }

    ppdb_data_t* value = result;
    if (value) {
        printf("Value: %.*s\n", (int)value->size,
               value->size <= sizeof(value->inline_data) ?
               value->inline_data : value->extended_data);
    }
}

static ppdb_error_t handle_get(int argc, char** argv,
                             const char* host, uint16_t port,
                             uint32_t timeout, bool nodelay) {
    if (argc < 3) {
        fprintf(stderr, "Missing key argument\n");
        return PPDB_ERR_PARAM;
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
        .use_tcp_nodelay = nodelay
    };

    // Connect to server
    ppdb_conn_t conn;
    err = ppdb_client_connect(ctx, &config, &conn);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to connect: %d\n", err);
        ppdb_destroy(ctx);
        return err;
    }

    // Prepare key
    ppdb_data_t key = {
        .size = strlen(argv[2]),
        .flags = 0
    };
    memcpy(key.inline_data, argv[2], key.size);

    // Get value
    err = ppdb_client_get(conn, &key, on_operation_complete, NULL);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to get value: %d\n", err);
    }

    // Wait for completion
    usleep(100000);  // 100ms

    // Cleanup
    ppdb_client_disconnect(conn);
    ppdb_destroy(ctx);
    return err;
}

static ppdb_error_t handle_put(int argc, char** argv,
                             const char* host, uint16_t port,
                             uint32_t timeout, bool nodelay) {
    if (argc < 4) {
        fprintf(stderr, "Missing key/value arguments\n");
        return PPDB_ERR_PARAM;
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
        .use_tcp_nodelay = nodelay
    };

    // Connect to server
    ppdb_conn_t conn;
    err = ppdb_client_connect(ctx, &config, &conn);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to connect: %d\n", err);
        ppdb_destroy(ctx);
        return err;
    }

    // Prepare key/value
    ppdb_data_t key = {
        .size = strlen(argv[2]),
        .flags = 0
    };
    memcpy(key.inline_data, argv[2], key.size);

    ppdb_data_t value = {
        .size = strlen(argv[3]),
        .flags = 0
    };
    memcpy(value.inline_data, argv[3], value.size);

    // Put value
    err = ppdb_client_put(conn, &key, &value, on_operation_complete, NULL);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to put value: %d\n", err);
    }

    // Wait for completion
    usleep(100000);  // 100ms

    // Cleanup
    ppdb_client_disconnect(conn);
    ppdb_destroy(ctx);
    return err;
}

static ppdb_error_t handle_delete(int argc, char** argv,
                                const char* host, uint16_t port,
                                uint32_t timeout, bool nodelay) {
    if (argc < 3) {
        fprintf(stderr, "Missing key argument\n");
        return PPDB_ERR_PARAM;
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
        .use_tcp_nodelay = nodelay
    };

    // Connect to server
    ppdb_conn_t conn;
    err = ppdb_client_connect(ctx, &config, &conn);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to connect: %d\n", err);
        ppdb_destroy(ctx);
        return err;
    }

    // Prepare key
    ppdb_data_t key = {
        .size = strlen(argv[2]),
        .flags = 0
    };
    memcpy(key.inline_data, argv[2], key.size);

    // Delete value
    err = ppdb_client_delete(conn, &key, on_operation_complete, NULL);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to delete value: %d\n", err);
    }

    // Wait for completion
    usleep(100000);  // 100ms

    // Cleanup
    ppdb_client_disconnect(conn);
    ppdb_destroy(ctx);
    return err;
}

//-----------------------------------------------------------------------------
// Command Entry
//-----------------------------------------------------------------------------

ppdb_error_t cmd_client(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return PPDB_ERR_PARAM;
    }

    // Parse options
    const char* host = "127.0.0.1";
    uint16_t port = 11211;
    uint32_t timeout = 1000;
    bool nodelay = false;

    for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "--host=", 7) == 0) {
            host = argv[i] + 7;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            port = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--timeout=", 10) == 0) {
            timeout = atoi(argv[i] + 10);
        } else if (strcmp(argv[i], "--nodelay") == 0) {
            nodelay = true;
        }
    }

    // Handle command
    const char* cmd = argv[1];
    if (strcmp(cmd, "get") == 0) {
        return handle_get(argc, argv, host, port, timeout, nodelay);
    } else if (strcmp(cmd, "put") == 0) {
        return handle_put(argc, argv, host, port, timeout, nodelay);
    } else if (strcmp(cmd, "delete") == 0) {
        return handle_delete(argc, argv, host, port, timeout, nodelay);
    } else if (strcmp(cmd, "--help") == 0) {
        print_usage();
        return PPDB_OK;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage();
        return PPDB_ERR_PARAM;
    }
} 