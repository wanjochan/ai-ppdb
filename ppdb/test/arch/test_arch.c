#include <stdio.h>
#include <assert.h>
#include "ppdb/PpdbArch.h"
#include "ppdb/PpdbInfra.h"
#include "internal/infra/InfraCore.h"

// Test InfraCore functionality
void test_infra_core() {
    InfraCore* core = infra_core_new();
    assert(core != NULL);
    assert(core->print != NULL);
    
    // Test print method
    core->print(core);
    
    // Cleanup
    core->free(core);
    printf("InfraCore tests passed\n");
}

// Test PpdbInfra functionality
void test_ppdb_infra() {
    PpdbInfra* infra = ppdb_infra_new();
    assert(infra != NULL);
    assert(infra->core != NULL);
    
    // Test core access
    infra->core->print(infra->core);
    
    // Cleanup
    infra->free(infra);
    printf("PpdbInfra tests passed\n");
}

// Test Ppdb functionality
void test_ppdb() {
    Ppdb* ppdb = ppdb_new();
    assert(ppdb != NULL);
    assert(ppdb->infra != NULL);
    
    // Test component access
    ppdb->infra->core->print(ppdb->infra->core);
    
    // Cleanup
    ppdb->free(ppdb);
    printf("Ppdb tests passed\n");
}

int main() {
    printf("Starting architecture tests...\n");
    
    test_infra_core();
    test_ppdb_infra();
    test_ppdb();
    
    printf("All tests passed!\n");
    return 0;
} 