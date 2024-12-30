#include "cosmopolitan.h"
#include "kvstore/internal/sync.h"
#include "ppdb/ppdb_error.h"

static int test_mutex(void) {
    printf("Testing mutex...\n");
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX
    };
    
    if (ppdb_sync_init(&sync, &config) != PPDB_OK) {
        printf("Failed to init mutex\n");
        return 1;
    }

    if (ppdb_sync_lock(&sync) != PPDB_OK) {
        printf("Failed to lock mutex\n");
        ppdb_sync_destroy(&sync);
        return 1;
    }

    if (ppdb_sync_unlock(&sync) != PPDB_OK) {
        printf("Failed to unlock mutex\n");
        ppdb_sync_destroy(&sync);
        return 1;
    }

    if (ppdb_sync_destroy(&sync) != PPDB_OK) {
        printf("Failed to destroy mutex\n");
        return 1;
    }

    printf("Mutex test passed\n");
    return 0;
}

static int test_spinlock(void) {
    printf("Testing spinlock...\n");
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_SPINLOCK
    };
    
    if (ppdb_sync_init(&sync, &config) != PPDB_OK) {
        printf("Failed to init spinlock\n");
        return 1;
    }

    if (ppdb_sync_lock(&sync) != PPDB_OK) {
        printf("Failed to lock spinlock\n");
        ppdb_sync_destroy(&sync);
        return 1;
    }

    if (ppdb_sync_unlock(&sync) != PPDB_OK) {
        printf("Failed to unlock spinlock\n");
        ppdb_sync_destroy(&sync);
        return 1;
    }

    if (ppdb_sync_destroy(&sync) != PPDB_OK) {
        printf("Failed to destroy spinlock\n");
        return 1;
    }

    printf("Spinlock test passed\n");
    return 0;
}

static int test_rwlock(void) {
    printf("Testing rwlock...\n");
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK
    };
    
    if (ppdb_sync_init(&sync, &config) != PPDB_OK) {
        printf("Failed to init rwlock\n");
        return 1;
    }

    if (ppdb_sync_lock(&sync) != PPDB_OK) {
        printf("Failed to read lock rwlock\n");
        ppdb_sync_destroy(&sync);
        return 1;
    }

    if (ppdb_sync_unlock(&sync) != PPDB_OK) {
        printf("Failed to read unlock rwlock\n");
        ppdb_sync_destroy(&sync);
        return 1;
    }

    if (ppdb_sync_lock(&sync) != PPDB_OK) {
        printf("Failed to write lock rwlock\n");
        ppdb_sync_destroy(&sync);
        return 1;
    }

    if (ppdb_sync_unlock(&sync) != PPDB_OK) {
        printf("Failed to write unlock rwlock\n");
        ppdb_sync_destroy(&sync);
        return 1;
    }

    if (ppdb_sync_destroy(&sync) != PPDB_OK) {
        printf("Failed to destroy rwlock\n");
        return 1;
    }

    printf("RWLock test passed\n");
    return 0;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("Running sync tests...\n");
    
    int result = 0;
    result |= test_mutex();
    result |= test_spinlock();
    result |= test_rwlock();
    
    printf("Sync tests completed with result: %d\n", result);
    return result;
} 