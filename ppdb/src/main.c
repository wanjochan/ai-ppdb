#include <cosmopolitan.h>
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/logger.h"
#include "ppdb/kvstore.h"

// Print help information
static void print_usage(void) {
    printf("PPDB - High Performance Key-Value Storage Engine\n");
    printf("\nUsage:\n");
    printf("  ppdb [options] <command> [arguments...]\n");
    printf("\nOptions:\n");
    printf("  --mode <locked|lockfree>  Operation mode (default: locked)\n");
    printf("  --dir <path>             Data directory path (default: db)\n");
    printf("  --memtable-size <bytes>  Memtable size (default: 1MB)\n");
    printf("  --l0-size <bytes>        L0 file size (default: 1MB)\n");
    printf("  --adaptive <on|off>      Enable/disable adaptive sharding (default: on)\n");
    printf("  --help                   Show this help message\n");
    printf("\nCommands:\n");
    printf("  put <key> <value>        Store a key-value pair\n");
    printf("  get <key>                Get value by key\n");
    printf("  delete <key>             Delete a key-value pair\n");
    printf("  list                     List all key-value pairs\n");
    printf("  stats                    Show database statistics\n");
    printf("  server                   Start HTTP API server\n");
    printf("\nExamples:\n");
    printf("  ppdb --mode lockfree put mykey myvalue\n");
    printf("  ppdb get mykey\n");
    printf("  ppdb --dir /path/to/db server\n");
}

// Command line options structure
typedef struct {
    ppdb_mode_t mode;
    char dir_path[256];
    size_t memtable_size;
    size_t l0_size;
    bool adaptive_sharding;
    char command[32];
    char key[256];
    char value[1024];
} cli_options_t;

// Parse command line arguments
static void parse_options(int argc, char* argv[], cli_options_t* opts) {
    // Set default values
    opts->mode = PPDB_MODE_LOCKED;
    strncpy(opts->dir_path, "db", sizeof(opts->dir_path) - 1);
    opts->memtable_size = 1024 * 1024;  // 1MB
    opts->l0_size = 1024 * 1024;        // 1MB
    opts->adaptive_sharding = true;
    memset(opts->command, 0, sizeof(opts->command));
    memset(opts->key, 0, sizeof(opts->key));
    memset(opts->value, 0, sizeof(opts->value));

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            exit(0);
        }
        else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "lockfree") == 0) {
                opts->mode = PPDB_MODE_LOCKFREE;
            }
            i++;
        }
        else if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            strncpy(opts->dir_path, argv[i + 1], sizeof(opts->dir_path) - 1);
            i++;
        }
        else if (strcmp(argv[i], "--memtable-size") == 0 && i + 1 < argc) {
            opts->memtable_size = atol(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--l0-size") == 0 && i + 1 < argc) {
            opts->l0_size = atol(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--adaptive") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "off") == 0) {
                opts->adaptive_sharding = false;
            }
            i++;
        }
        else if (opts->command[0] == '\0') {
            strncpy(opts->command, argv[i], sizeof(opts->command) - 1);
        }
        else if (opts->key[0] == '\0') {
            strncpy(opts->key, argv[i], sizeof(opts->key) - 1);
        }
        else if (opts->value[0] == '\0') {
            strncpy(opts->value, argv[i], sizeof(opts->value) - 1);
        }
    }
}

// Execute command
static int execute_command(ppdb_kvstore_t* store, const cli_options_t* opts) {
    ppdb_error_t err;
    
    if (strcmp(opts->command, "put") == 0) {
        if (opts->key[0] == '\0' || opts->value[0] == '\0') {
            printf("Error: put command requires key and value arguments\n");
            return 1;
        }
        err = ppdb_kvstore_put(store, 
                              (const uint8_t*)opts->key, strlen(opts->key),
                              (const uint8_t*)opts->value, strlen(opts->value));
        if (err != PPDB_OK) {
            printf("Error storing key-value pair: %s\n", ppdb_error_string(err));
            return 1;
        }
        printf("Successfully stored key-value pair\n");
    }
    else if (strcmp(opts->command, "get") == 0) {
        if (opts->key[0] == '\0') {
            printf("Error: get command requires key argument\n");
            return 1;
        }
        uint8_t* value = NULL;
        size_t value_size = 0;
        err = ppdb_kvstore_get(store, 
                              (const uint8_t*)opts->key, strlen(opts->key),
                              &value, &value_size);
        if (err == PPDB_ERR_NOT_FOUND) {
            printf("Key not found\n");
            return 1;
        }
        if (err != PPDB_OK) {
            printf("Error getting value: %s\n", ppdb_error_string(err));
            return 1;
        }
        printf("%.*s\n", (int)value_size, value);
        free(value);
    }
    else if (strcmp(opts->command, "delete") == 0) {
        if (opts->key[0] == '\0') {
            printf("Error: delete command requires key argument\n");
            return 1;
        }
        err = ppdb_kvstore_delete(store, 
                                 (const uint8_t*)opts->key, strlen(opts->key));
        if (err != PPDB_OK) {
            printf("Error deleting key: %s\n", ppdb_error_string(err));
            return 1;
        }
        printf("Successfully deleted key\n");
    }
    else if (strcmp(opts->command, "stats") == 0) {
        printf("Database statistics:\n");
        printf("- Operation mode: %s\n", opts->mode == PPDB_MODE_LOCKFREE ? "lock-free" : "locked");
        printf("- Data directory: %s\n", opts->dir_path);
        printf("- Memtable size limit: %zu bytes\n", opts->memtable_size);
        printf("- L0 file size limit: %zu bytes\n", opts->l0_size);
        printf("- Adaptive sharding: %s\n", opts->adaptive_sharding ? "on" : "off");
        // TODO: Add more statistics
    }
    else if (strcmp(opts->command, "server") == 0) {
        printf("HTTP API server functionality not implemented\n");
        return 1;
    }
    else {
        printf("Error: unknown command '%s'\n", opts->command);
        print_usage();
        return 1;
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    // Show help if no arguments
    if (argc == 1) {
        print_usage();
        return 0;
    }

    // Parse command line options
    cli_options_t opts;
    parse_options(argc, argv, &opts);

    // Check if command is specified
    if (opts.command[0] == '\0') {
        printf("Error: no command specified\n");
        print_usage();
        return 1;
    }

    ppdb_log_info("PPDB starting...");
    ppdb_log_info("Running in %s mode", opts.mode == PPDB_MODE_LOCKFREE ? "lock-free" : "locked");

    // Create KVStore configuration
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = opts.memtable_size,
        .l0_size = opts.l0_size,
        .l0_files = 4,
        .compression = PPDB_COMPRESSION_NONE,
        .mode = opts.mode,
        .adaptive_sharding = opts.adaptive_sharding
    };
    
    // Safely copy directory path
    snprintf(config.dir_path, sizeof(config.dir_path), "%s", opts.dir_path);

    // Create KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create KVStore: %s", ppdb_error_string(err));
        return 1;
    }

    ppdb_log_info("PPDB started successfully");

    // Execute command
    int ret = execute_command(store, &opts);

    // Clean up
    ppdb_kvstore_destroy(store);

    return ret;
}