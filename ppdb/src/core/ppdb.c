#include "ppdb/ppdb.h"
#include <stdio.h>
#include <string.h>

static void print_usage(void) {
    printf("Usage: ppdb <command> [options]\n");
    printf("\nCommands:\n");
    printf("  open <path>      Open database at path\n");
    printf("  get <key>        Get value by key\n");
    printf("  put <key> <val>  Put key-value pair\n");
    printf("  del <key>        Delete key\n");
    printf("  stats            Show database statistics\n");
    printf("  help             Show this help message\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* cmd = argv[1];
    ppdb_kvstore_t* db = NULL;
    ppdb_error_t err;

    if (strcmp(cmd, "help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(cmd, "open") == 0) {
        if (argc != 3) {
            printf("Error: open command requires path argument\n");
            return 1;
        }
        err = ppdb_open(argv[2], &db);
        if (err != PPDB_OK) {
            printf("Error opening database: %s\n", ppdb_error_string(err));
            return 1;
        }
        printf("Database opened successfully\n");
        ppdb_close(db);
        return 0;
    }

    // 其他命令都需要先打开数据库
    if (argc < 3) {
        printf("Error: database path required\n");
        return 1;
    }

    err = ppdb_open(argv[2], &db);
    if (err != PPDB_OK) {
        printf("Error opening database: %s\n", ppdb_error_string(err));
        return 1;
    }

    if (strcmp(cmd, "get") == 0) {
        if (argc != 4) {
            printf("Error: get command requires key argument\n");
            ppdb_close(db);
            return 1;
        }
        ppdb_key_t key = {argv[3], strlen(argv[3])};
        ppdb_value_t value = {NULL, 0};
        err = ppdb_get(db, &key, &value);
        if (err != PPDB_OK) {
            printf("Error getting value: %s\n", ppdb_error_string(err));
        } else {
            printf("%.*s\n", (int)value.size, (char*)value.data);
            free(value.data);
        }
    }
    else if (strcmp(cmd, "put") == 0) {
        if (argc != 5) {
            printf("Error: put command requires key and value arguments\n");
            ppdb_close(db);
            return 1;
        }
        ppdb_key_t key = {argv[3], strlen(argv[3])};
        ppdb_value_t value = {argv[4], strlen(argv[4])};
        err = ppdb_put(db, &key, &value);
        if (err != PPDB_OK) {
            printf("Error putting value: %s\n", ppdb_error_string(err));
        } else {
            printf("Value stored successfully\n");
        }
    }
    else if (strcmp(cmd, "del") == 0) {
        if (argc != 4) {
            printf("Error: del command requires key argument\n");
            ppdb_close(db);
            return 1;
        }
        ppdb_key_t key = {argv[3], strlen(argv[3])};
        err = ppdb_remove(db, &key);
        if (err != PPDB_OK) {
            printf("Error removing key: %s\n", ppdb_error_string(err));
        } else {
            printf("Key removed successfully\n");
        }
    }
    else if (strcmp(cmd, "stats") == 0) {
        ppdb_storage_stats_t stats;
        err = ppdb_get_stats(db, &stats);
        if (err != PPDB_OK) {
            printf("Error getting stats: %s\n", ppdb_error_string(err));
        } else {
            printf("Database Statistics:\n");
            printf("Total Keys: %lu\n", stats.base_metrics.total_keys);
            printf("Total Bytes: %lu\n", stats.base_metrics.total_bytes);
            printf("Get Operations: %lu (hits: %lu)\n",
                   stats.base_metrics.get_count,
                   stats.base_metrics.get_hits);
            printf("Put Operations: %lu\n", stats.base_metrics.put_count);
            printf("Remove Operations: %lu\n", stats.base_metrics.remove_count);
            printf("Memory Used: %lu bytes\n", stats.memory_used);
            printf("Memory Allocated: %lu bytes\n", stats.memory_allocated);
            printf("Block Count: %lu\n", stats.block_count);
        }
    }
    else {
        printf("Unknown command: %s\n", cmd);
        print_usage();
        ppdb_close(db);
        return 1;
    }

    ppdb_close(db);
    return err == PPDB_OK ? 0 : 1;
} 