#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "test_common.h"

//-----------------------------------------------------------------------------
// Test Data
//-----------------------------------------------------------------------------

#define TEST_PORT 11211
#define TEST_THREADS 4
#define TEST_CONNECTIONS 10
#define TEST_ITERATIONS 1000

typedef struct {
    ppdb_peer_t peer;
    int id;
    int success;
    int failure;
} peer_context_t;

//-----------------------------------------------------------------------------
// Test Callbacks
//-----------------------------------------------------------------------------

static void on_peer_event(ppdb_peer_event_t event, void* user_data) {
    peer_context_t* ctx = user_data;
    switch(event.type) {
        case PPDB_EVENT_CONNECTED:
            ctx->success++;
            break;
        case PPDB_EVENT_ERROR:
            ctx->failure++;
            break;
    }
}

static void on_operation_complete(ppdb_result_t result, void* user_data) {
    peer_context_t* ctx = user_data;
    if (result.status == PPDB_OK) {
        ctx->success++;
    } else {
        ctx->failure++;
    }
}

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

static void test_concurrent_operations(void) {
    // Create server peer
    ppdb_peer_config_t server_config = {
        .mode = PPDB_PEER_SERVER,
        .db_path = "test_data",
        .cache_size = 1024 * 1024 * 1024,
        .max_connections = TEST_CONNECTIONS,
        .io_threads = TEST_THREADS,
        .host = "127.0.0.1",
        .port = TEST_PORT,
        .on_event = on_peer_event
    };

    ppdb_peer_t server;
    TEST_ASSERT(ppdb_peer_create(&server, &server_config) == PPDB_OK);

    // Create client peers
    peer_context_t peers[TEST_CONNECTIONS];
    memset(peers, 0, sizeof(peers));

    for (int i = 0; i < TEST_CONNECTIONS; i++) {
        ppdb_peer_config_t peer_config = {
            .mode = PPDB_PEER_CLIENT,
            .host = "127.0.0.1", 
            .port = TEST_PORT,
            .io_threads = 1,
            .on_event = on_peer_event
        };

        TEST_ASSERT(ppdb_peer_create(&peers[i].peer, &peer_config) == PPDB_OK);
        peers[i].id = i;
    }

    // Run concurrent operations
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        for (int j = 0; j < TEST_CONNECTIONS; j++) {
            // Prepare data
            char key_buf[32];
            char value_buf[32];
            snprintf(key_buf, sizeof(key_buf), "key_%d_%d", j, i);
            snprintf(value_buf, sizeof(value_buf), "value_%d_%d", j, i);

            ppdb_data_t key = {
                .size = strlen(key_buf),
                .flags = 0
            };
            memcpy(key.inline_data, key_buf, key.size);

            ppdb_data_t value = {
                .size = strlen(value_buf),
                .flags = 0
            };
            memcpy(value.inline_data, value_buf, value.size);

            // Operations using peer interface
            TEST_ASSERT(ppdb_peer_put(peers[j].peer, &key, &value,
                                    on_operation_complete, &peers[j]) == PPDB_OK);

            TEST_ASSERT(ppdb_peer_get(peers[j].peer, &key,
                                    on_operation_complete, &peers[j]) == PPDB_OK);

            TEST_ASSERT(ppdb_peer_delete(peers[j].peer, &key,
                                       on_operation_complete, &peers[j]) == PPDB_OK);
        }
    }

    // Check results and cleanup
    for (int i = 0; i < TEST_CONNECTIONS; i++) {
        printf("Peer %d: %d success, %d failure\n",
               i, peers[i].success, peers[i].failure);
        TEST_ASSERT(peers[i].success > 0);
        TEST_ASSERT(peers[i].failure == 0);

        TEST_ASSERT(ppdb_peer_destroy(peers[i].peer) == PPDB_OK);
    }

    // Get stats and cleanup server
    char stats[1024];
    TEST_ASSERT(ppdb_peer_get_stats(server, stats, sizeof(stats)) == PPDB_OK);
    printf("Server Stats:\n%s\n", stats);

    TEST_ASSERT(ppdb_peer_destroy(server) == PPDB_OK);
}

//-----------------------------------------------------------------------------
// Test Runner
//-----------------------------------------------------------------------------

int main(void) {
    TEST_INIT();

    TEST_RUN(test_concurrent_operations);

    TEST_CLEANUP();
    return 0;
}