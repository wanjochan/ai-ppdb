#include <stdio.h>
#include <assert.h>
#include "ppdb/PpxArch.h"
#include "ppdb/PpxInfra.h"
#include "internal/infrax/InfraxCore.h"

// Test InfraxCore functionality
void test_infrax_core() {
    InfraxCore* core = infrax_core_new();
    assert(core != NULL);
    assert(core->print != NULL);
    
    core->print(core);
    
    // Cleanup
    core->free(core);
    printf("InfraxCore tests passed\n");
}

// Test PpxInfra functionality
void test_ppx_infra() {
    PpxInfra* infra = ppx_infra_new();
    assert(infra != NULL);
    assert(infra->core != NULL);
    assert(infra->core->print != NULL);
    
    // Test core access
    infra->core->print(infra->core);
    
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
    
    // Test component access
    arch->infra->core->print(arch->infra->core);
    
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