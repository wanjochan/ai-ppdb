#include <stdio.h>
#include <assert.h>
#include "ppdb/PpxArch.h"
#include "ppdb/PpxInfra.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

// Test InfraxCore functionality
void test_infrax_core() {
    InfraxCore* core = infrax_core_new();
    assert(core != NULL);
    
    // Cleanup
    core->free(core);
    printf("InfraxCore tests passed\n");
}

// Test PpxInfra functionality
void test_ppx_infra() {
    PpxInfra* infra = ppx_infra_new();
    assert(infra != NULL);
    assert(infra->core != NULL);
    assert(infra->logger != NULL);
    
    // Test logging methods
    infra->logger->info(infra->logger, "Testing PpxInfra logging: %s", "INFO");
    infra->logger->warn(infra->logger, "Testing PpxInfra logging: %s", "WARN");
    infra->logger->error(infra->logger, "Testing PpxInfra logging: %s", "ERROR");
    
    // Cleanup
    infra->free(infra);
    printf("PpxInfra tests passed\n");
}

// Test PpxArch functionality
void test_ppx_arch() {
    PpxArch* arch = ppx_arch_new();
    assert(arch != NULL);
    assert(arch->infra != NULL);
    assert(arch->infra->core != NULL);
    assert(arch->infra->logger != NULL);
    
    // Test logging through architecture
    arch->infra->logger->info(arch->infra->logger, "Testing PpxArch logging: %s", "INFO");
    arch->infra->logger->warn(arch->infra->logger, "Testing PpxArch logging: %s", "WARN");
    arch->infra->logger->error(arch->infra->logger, "Testing PpxArch logging: %s", "ERROR");
    
    // Cleanup
    arch->free(arch);
    printf("PpxArch tests passed\n");
}

int main() {
    printf("Starting architecture tests...\n");
    
    test_infrax_core();
    test_ppx_infra();
    test_ppx_arch();
    
    printf("All tests passed!\n");
    return 0;
}
