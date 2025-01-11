#include <ppdb/ppdb.h>
#include "../../../src/internal/peer.h"
#include "../test_framework.h"
#include "../framework/test_framework.h"
#include "internal/infra/infra.h"

// Test peer initialization and cleanup
int main(void) {
    // Test initialization
    TEST_ASSERT(peer_init() == PPDB_OK);
    TEST_ASSERT(peer_is_initialized() == true);

    // Test double initialization
    TEST_ASSERT(peer_init() == PPDB_OK);
    TEST_ASSERT(peer_is_initialized() == true);

    // Test cleanup
    peer_cleanup();
    TEST_ASSERT(peer_is_initialized() == false);

    // Test double cleanup
    peer_cleanup();
    TEST_ASSERT(peer_is_initialized() == false);

    return 0;
} 