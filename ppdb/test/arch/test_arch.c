#include "cosmopolitan.h"
#include <assert.h>
#include "internal/arch/PpxArch.h"
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

void test_ppx_arch(void) {
    // Test instance creation
    PpxArch* arch = PpxArch_CLASS.new();
    assert(arch != NULL);
    assert(arch->infra != NULL);
    assert(arch->infra->core != NULL);
    assert(arch->infra->logger != NULL);
    assert(arch->infra->logger->klass == &InfraxLog_CLASS);
    
    // Test logging through architecture
    arch->infra->logger->info(arch->infra->logger, "Testing PpxArch logging: %s", "INFO");
    arch->infra->logger->warn(arch->infra->logger, "Testing PpxArch logging: %s", "WARN");
    arch->infra->logger->error(arch->infra->logger, "Testing PpxArch logging: %s", "ERROR");
    
    // Test instance cleanup
    PpxArch_CLASS.free(arch);
    
    // Test global instance
    PpxArch* global_arch = get_global_ppxArch();
    assert(global_arch != NULL);
    assert(global_arch->infra != NULL);
    assert(global_arch->klass == &PpxArch_CLASS);
    assert(global_arch->infra->logger->klass == &InfraxLog_CLASS);
    
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
