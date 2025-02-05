#include "cosmopolitan.h"
#include <assert.h>
#include "ppdb/PpxArch.h"

void test_infrax_core(void) {
    InfraxCore* core = get_global_infrax_core();
    assert(core != NULL);
    //core->time_now(core);
    core->printf(core, "time_now_ms=%d",core->time_now_ms(core));
    core->printf(core, "time_monotonic_ms=%d",core->time_monotonic_ms(core));
    core->printf(core, "InfraxCore tests passed\n");
}

void test_ppx_infra(void) {
    PpxInfra* infra = get_global_ppxInfra();
    assert(infra != NULL);
    assert(infra->core != NULL);
    assert(infra->logger != NULL);
    
    // Test logging functionality
    infra->logger->info(infra->logger, "Testing PpxInfra logging: %s", "INFO");
    infra->logger->warn(infra->logger, "Testing PpxInfra logging: %s", "WARN");
    infra->logger->error(infra->logger, "Testing PpxInfra logging: %s", "ERROR");
    
    printf("PpxInfra tests passed\n");
}

void test_ppx_arch(void) {
    PpxArch* arch = get_global_ppxArch();
    assert(arch != NULL);
    assert(arch->infra != NULL);
    assert(arch->infra->core != NULL);
    assert(arch->infra->logger != NULL);
    
    // Test logging through architecture
    arch->infra->logger->info(arch->infra->logger, "Testing PpxArch logging: %s", "INFO");
    arch->infra->logger->warn(arch->infra->logger, "Testing PpxArch logging: %s", "WARN");
    arch->infra->logger->error(arch->infra->logger, "Testing PpxArch logging: %s", "ERROR");
    
    printf("PpxArch tests passed\n");
}

int main(void) {
    printf("Starting architecture tests...\n");
    
    test_infrax_core();
    test_ppx_infra();
    test_ppx_arch();
    
    printf("All tests passed!\n");
    return 0;
}
