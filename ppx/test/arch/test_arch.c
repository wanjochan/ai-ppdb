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
    core->printf(core, "time_monotonic_ms=%d\n",core->time_monotonic_ms(core));
    
    // Test yield functionality
    core->printf(core, "Testing yield...\n");
    core->yield(core);
    core->printf(core, "Yield test completed\n");
    
    // Test pid functionality
    core->printf(core, "Testing pid...\n");
    int process_id = core->pid(core);
    core->printf(core, "Current process id: %d\n", process_id);
    assert(process_id > 0);  // Process ID should always be positive
    core->printf(core, "Pid test completed\n");
    
    // Test network byte order conversion
    core->printf(core, "Testing network byte order conversion...\n");
    
    // Test 16-bit conversion
    uint16_t host16 = 0x1234;
    uint16_t net16 = core->host_to_net16(core, host16);
    assert(core->net_to_host16(core, net16) == host16);
    core->printf(core, "16-bit conversion test passed\n");
    
    // Test 32-bit conversion
    uint32_t host32 = 0x12345678;
    uint32_t net32 = core->host_to_net32(core, host32);
    assert(core->net_to_host32(core, net32) == host32);
    core->printf(core, "32-bit conversion test passed\n");
    
    // Test 64-bit conversion
    uint64_t host64 = 0x1234567890ABCDEF;
    uint64_t net64 = core->host_to_net64(core, host64);
    assert(core->net_to_host64(core, net64) == host64);
    core->printf(core, "64-bit conversion test passed\n");
    
    core->printf(core, "Network byte order conversion tests passed\n");
    
    core->printf(core, "InfraxCore tests passed\n");
}

void test_ppx_infra(void) {
    // Test global instance
    const PpxInfra* infra = ppx_infra();
    assert(infra != NULL);
    assert(infra->core != NULL);
    assert(infra->logger != NULL);
    
    // Test logging functionality
    infra->logger->info(infra->logger, "Testing PpxInfra logging: %s", "INFO");
    infra->logger->warn(infra->logger, "Testing PpxInfra logging: %s", "WARN");
    infra->logger->error(infra->logger, "Testing PpxInfra logging: %s", "ERROR");
    
    // Get instance again to test singleton behavior
    const PpxInfra* infra2 = ppx_infra();
    assert(infra2 == infra);  // Should be the same instance
    assert(infra2->core == infra->core);
    assert(infra2->logger == infra->logger);
    
    printf("PpxInfra tests passed\n");
}

static void test_string_operations(void) {
    InfraxCore* core = get_global_infrax_core();
    
    // Test strlen
    const char* test_str = "Hello, World!";
    assert(core->strlen(core, test_str) == 13);
    
    // Test strcpy and strcmp
    char dest[20];
    core->strcpy(core, dest, test_str);
    assert(core->strcmp(core, dest, test_str) == 0);
    
    // Test strncpy
    char dest2[10];
    core->strncpy(core, dest2, test_str, 5);
    dest2[5] = '\0';
    assert(core->strcmp(core, dest2, "Hello") == 0);
    
    // Test strcat
    char concat_dest[30] = "Hello, ";
    core->strcat(core, concat_dest, "World!");
    assert(core->strcmp(core, concat_dest, "Hello, World!") == 0);
    
    // Test strncat
    char ncat_dest[30] = "Hello";
    core->strncat(core, ncat_dest, ", World!", 2);
    assert(core->strcmp(core, ncat_dest, "Hello, ") == 0);
    
    // Test strchr and strrchr
    const char* str_with_multiple_a = "banana";
    assert(*core->strchr(core, str_with_multiple_a, 'a') == 'a');
    assert(core->strchr(core, str_with_multiple_a, 'a') == str_with_multiple_a + 1);
    assert(core->strrchr(core, str_with_multiple_a, 'a') == str_with_multiple_a + 5);
    
    // Test strstr
    const char* haystack = "Hello, World!";
    assert(core->strstr(core, haystack, "World") == haystack + 7);
    assert(core->strstr(core, haystack, "notfound") == NULL);
    
    // Test strdup and strndup
    char* dup_str = core->strdup(core, test_str);
    assert(core->strcmp(core, dup_str, test_str) == 0);
    free(dup_str);
    
    char* ndup_str = core->strndup(core, test_str, 5);
    assert(core->strlen(core, ndup_str) == 5);
    assert(core->strncmp(core, ndup_str, "Hello", 5) == 0);
    free(ndup_str);
    
    printf("String operations tests passed!\n");
}

int main(void) {
    printf("Starting architecture tests...\n");
    
    test_infrax_core();
    test_string_operations();
    test_ppx_infra();
    
    printf("All tests passed!\n");
    return 0;
}
