#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "cmd/cmd_internal.h"

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