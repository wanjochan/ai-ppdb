#include "cosmopolitan.h"
#include <assert.h>
#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxLog.h"

void test_infrax_core(void) {
    InfraxCore* core = get_global_infrax_core();
    assert(core != NULL);
    //core->time_now(core);
    core->printf(core, "time_now_ms=%d\n",core->time_now_ms(core));
    core->printf(core, "sleep_ms 100\n");
    core->sleep_ms(NULL,100);
    core->printf(core, "time_monotonic_ms=%d",core->time_monotonic_ms(core));
    core->printf(core, "InfraxCore tests passed\n");
}

void test_ppx_infra(void) {
    // Test instance creation
    PpxInfra* infra = PpxInfra_CLASS.new();
    assert(infra != NULL);
    assert(infra->core != NULL);
    assert(infra->logger != NULL);
    assert(infra->klass == &PpxInfra_CLASS);
    assert(infra->logger->klass == &InfraxLog_CLASS);
    
    // Test logging functionality
    infra->logger->info(infra->logger, "Testing PpxInfra logging: %s", "INFO");
    infra->logger->warn(infra->logger, "Testing PpxInfra logging: %s", "WARN");
    infra->logger->error(infra->logger, "Testing PpxInfra logging: %s", "ERROR");
    
    // Test instance cleanup
    PpxInfra_CLASS.free(infra);
    
    // Test global instance
    PpxInfra* global_infra = get_global_ppxInfra();
    assert(global_infra != NULL);
    assert(global_infra->core != NULL);
    assert(global_infra->logger != NULL);
    assert(global_infra->klass == &PpxInfra_CLASS);
    assert(global_infra->logger->klass == &InfraxLog_CLASS);
    
    printf("PpxInfra tests passed\n");
}

int main(void) {
    printf("Starting architecture tests...\n");
    
    test_infrax_core();
    test_ppx_infra();
    
    printf("All tests passed!\n");
    return 0;
}
