#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxLog.h"

InfraxCore* core = NULL;
const PpxInfra* infra = NULL;

void test_infrax_core(void) {
    INFRAX_ASSERT(core, core != NULL);
    core->printf(core, "time_now_ms=%d\n",core->time_now_ms(core));
    core->printf(core, "sleep_ms 100\n");
    core->sleep_ms(NULL,100);
    core->printf(core, "time_monotonic_ms=%d\n",core->time_monotonic_ms(core));
    
    // Test pid functionality
    core->printf(core, "Testing pid...\n");
    int process_id = core->pid(core);
    core->printf(core, "Current process id: %d\n", process_id);
    INFRAX_ASSERT_MSG(core, process_id > 0, "Process ID should always be positive");
    core->printf(core, "Pid test completed\n");
    
    // Test network byte order conversion
    core->printf(core, "Testing network byte order conversion...\n");
    
    // Test 16-bit conversion
    uint16_t host16 = 0x1234;
    uint16_t net16 = core->host_to_net16(core, host16);
    INFRAX_ASSERT(core, core->net_to_host16(core, net16) == host16);
    core->printf(core, "16-bit conversion test passed\n");
    
    // Test 32-bit conversion
    uint32_t host32 = 0x12345678;
    uint32_t net32 = core->host_to_net32(core, host32);
    INFRAX_ASSERT(core, core->net_to_host32(core, net32) == host32);
    core->printf(core, "32-bit conversion test passed\n");
    
    // Test 64-bit conversion
    uint64_t host64 = 0x1234567890ABCDEF;
    uint64_t net64 = core->host_to_net64(core, host64);
    INFRAX_ASSERT(core, core->net_to_host64(core, net64) == host64);
    core->printf(core, "64-bit conversion test passed\n");
    
    core->printf(core, "Network byte order conversion tests passed\n");
    
    core->printf(core, "InfraxCore tests passed\n");
}

void test_ppx_infra(void) {
    INFRAX_ASSERT(core, infra != NULL);
    INFRAX_ASSERT(core, infra->core != NULL);
    INFRAX_ASSERT(core, infra->logger != NULL);
    
    // Test logging functionality
    infra->logger->info(infra->logger, "Testing PpxInfra logging: %s", "INFO");
    infra->logger->warn(infra->logger, "Testing PpxInfra logging: %s", "WARN");
    infra->logger->error(infra->logger, "Testing PpxInfra logging: %s", "ERROR");
    
    // Get instance again to test singleton behavior
    const PpxInfra* infra2 = ppx_infra();
    INFRAX_ASSERT_MSG(core, infra2 == infra, "Should be the same instance");
    INFRAX_ASSERT_MSG(core, infra2->core == infra->core, "Core should be the same");
    INFRAX_ASSERT_MSG(core, infra2->logger == infra->logger, "Logger should be the same");
    
    printf("PpxInfra tests passed\n");
}

static void test_string_operations(void) {
    
    // Test strlen
    const char* test_str = "Hello, World!";
    INFRAX_ASSERT(core, core->strlen(core, test_str) == 13);
    
    // Test strcpy and strcmp
    char dest[20];
    core->strcpy(core, dest, test_str);
    INFRAX_ASSERT(core, core->strcmp(core, dest, test_str) == 0);
    
    // Test strncpy
    char dest2[10];
    core->strncpy(core, dest2, test_str, 5);
    dest2[5] = '\0';
    INFRAX_ASSERT(core, core->strcmp(core, dest2, "Hello") == 0);
    
    // Test strcat
    char concat_dest[30] = "Hello, ";
    core->strcat(core, concat_dest, "World!");
    INFRAX_ASSERT(core, core->strcmp(core, concat_dest, "Hello, World!") == 0);
    
    // Test strncat
    char ncat_dest[30] = "Hello";
    core->strncat(core, ncat_dest, ", World!", 2);
    INFRAX_ASSERT(core, core->strcmp(core, ncat_dest, "Hello, ") == 0);
    
    // Test strchr and strrchr
    const char* str_with_multiple_a = "banana";
    INFRAX_ASSERT(core, *core->strchr(core, str_with_multiple_a, 'a') == 'a');
    INFRAX_ASSERT(core, core->strchr(core, str_with_multiple_a, 'a') == str_with_multiple_a + 1);
    INFRAX_ASSERT(core, core->strrchr(core, str_with_multiple_a, 'a') == str_with_multiple_a + 5);
    
    // Test strstr
    const char* haystack = "Hello, World!";
    INFRAX_ASSERT(core, core->strstr(core, haystack, "World") == haystack + 7);
    INFRAX_ASSERT(core, core->strstr(core, haystack, "notfound") == NULL);
    
    // Test strdup and strndup
    char* dup_str = core->strdup(core, test_str);
    INFRAX_ASSERT(core, core->strcmp(core, dup_str, test_str) == 0);
    free(dup_str);
    
    char* ndup_str = core->strndup(core, test_str, 5);
    INFRAX_ASSERT(core, core->strlen(core, ndup_str) == 5);
    INFRAX_ASSERT(core, core->strncmp(core, ndup_str, "Hello", 5) == 0);
    free(ndup_str);
    
    printf("String operations tests passed!\n");
}

static void test_time_operations(void) {
    
    // Test time_now_ms
    InfraxTime t1 = core->time_now_ms(core);
    core->sleep_ms(core, 10);  // Sleep for 10ms
    InfraxTime t2 = core->time_now_ms(core);
    INFRAX_ASSERT_MSG(core, t2 > t1, "Time should increase");
    
    // Test time_monotonic_ms
    InfraxTime m1 = core->time_monotonic_ms(core);
    core->sleep_ms(core, 10);  // Sleep for 10ms
    InfraxTime m2 = core->time_monotonic_ms(core);
    INFRAX_ASSERT_MSG(core, m2 > m1, "Monotonic time should increase");
    
    // Test sleep_ms precision
    InfraxTime start = core->time_monotonic_ms(core);
    core->sleep_ms(core, 100);  // Sleep for 100ms
    InfraxTime end = core->time_monotonic_ms(core);
    InfraxTime elapsed = end - start;
    // Allow for more scheduling variance on different systems
    INFRAX_ASSERT_MSG(core, elapsed >= 50 && elapsed <= 200, "Sleep duration should be within reasonable bounds");
    
    printf("Time operations tests passed!\n");
}

static void test_random_operations(void) {
    
    // Test random seed and generation
    core->random_seed(core, 12345);  // Set a fixed seed
    InfraxU32 r1 = core->random(core);
    InfraxU32 r2 = core->random(core);
    INFRAX_ASSERT_MSG(core, r1 != r2, "Two consecutive random numbers should be different");
    
    // Test reproducibility
    core->random_seed(core, 12345);  // Reset to same seed
    InfraxU32 r3 = core->random(core);
    INFRAX_ASSERT_MSG(core, r1 == r3, "First number should be same with same seed");
    
    printf("Random operations tests passed!\n");
}

static void test_buffer_operations(void) {
    InfraxBuffer buf;
    
    // Test buffer initialization
    InfraxError err = core->buffer_init(core, &buf, 16);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Buffer initialization should succeed");
    INFRAX_ASSERT(core, buf.capacity == 16);
    INFRAX_ASSERT(core, buf.size == 0);
    
    // Test buffer write
    const char* test_data = "Hello, World!";
    err = core->buffer_write(core, &buf, test_data, strlen(test_data));
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Buffer write should succeed");
    INFRAX_ASSERT(core, buf.size == strlen(test_data));
    
    // Test buffer read
    char read_data[16];
    err = core->buffer_read(core, &buf, read_data, strlen(test_data));
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Buffer read should succeed");
    INFRAX_ASSERT(core, memcmp(read_data, test_data, strlen(test_data)) == 0);
    INFRAX_ASSERT(core, buf.size == 0);
    
    // Test buffer reset
    err = core->buffer_write(core, &buf, test_data, strlen(test_data));
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Buffer write should succeed");
    core->buffer_reset(core, &buf);
    INFRAX_ASSERT(core, buf.size == 0);
    
    // Cleanup
    core->buffer_destroy(core, &buf);
    
    printf("Buffer operations tests passed!\n");
}

static void test_ring_buffer_operations(void) {
    InfraxRingBuffer rb;
    
    // Test ring buffer initialization
    InfraxError err = core->ring_buffer_init(core, &rb, 16);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Ring buffer initialization should succeed");
    INFRAX_ASSERT(core, rb.size == 16);
    INFRAX_ASSERT(core, !rb.full);
    
    // Test ring buffer write
    const char* test_data = "Hello";
    err = core->ring_buffer_write(core, &rb, test_data, strlen(test_data));
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Ring buffer write should succeed");
    INFRAX_ASSERT(core, core->ring_buffer_readable(core, &rb) == strlen(test_data));
    
    // Test ring buffer read
    char read_data[16];
    err = core->ring_buffer_read(core, &rb, read_data, strlen(test_data));
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Ring buffer read should succeed");
    INFRAX_ASSERT(core, memcmp(read_data, test_data, strlen(test_data)) == 0);
    INFRAX_ASSERT(core, core->ring_buffer_readable(core, &rb) == 0);
    
    // Test ring buffer wrap-around
    const char* test_data2 = "World";
    err = core->ring_buffer_write(core, &rb, test_data2, strlen(test_data2));
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Ring buffer write should succeed");
    err = core->ring_buffer_read(core, &rb, read_data, strlen(test_data2));
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "Ring buffer read should succeed");
    INFRAX_ASSERT(core, memcmp(read_data, test_data2, strlen(test_data2)) == 0);
    
    // Test ring buffer reset
    core->ring_buffer_reset(core, &rb);
    INFRAX_ASSERT(core, core->ring_buffer_readable(core, &rb) == 0);
    INFRAX_ASSERT(core, !rb.full);
    
    // Cleanup
    core->ring_buffer_destroy(core, &rb);
    
    printf("Ring buffer operations tests passed!\n");
}

static void test_file_operations(void) {
    InfraxHandle file;
    const char* test_path = "./test.txt";
    const char* test_data = "Hello, File I/O!";
    const char* new_path = "./test_renamed.txt";
    
    // Test file creation and write
    InfraxError err = core->file_open(core, test_path, INFRAX_FILE_CREATE | INFRAX_FILE_WRONLY | INFRAX_FILE_TRUNC, 0644, &file);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File open should succeed");
    
    size_t written;
    err = core->file_write(core, file, test_data, strlen(test_data), &written);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File write should succeed");
    INFRAX_ASSERT(core, written == strlen(test_data));
    
    err = core->file_close(core, file);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File close should succeed");
    
    // Test file read
    err = core->file_open(core, test_path, INFRAX_FILE_RDONLY, 0, &file);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File open for reading should succeed");
    
    char read_data[64] = {0};
    size_t bytes_read;
    err = core->file_read(core, file, read_data, sizeof(read_data), &bytes_read);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File read should succeed");
    INFRAX_ASSERT(core, bytes_read == strlen(test_data));
    INFRAX_ASSERT(core, memcmp(read_data, test_data, strlen(test_data)) == 0);
    
    err = core->file_close(core, file);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File close should succeed");
    
    // Test file rename
    err = core->file_rename(core, test_path, new_path);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File rename should succeed");
    
    // Test file exists
    bool exists;
    err = core->file_exists(core, new_path, &exists);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File exists check should succeed");
    INFRAX_ASSERT(core, exists);
    
    err = core->file_exists(core, test_path, &exists);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File exists check should succeed");
    INFRAX_ASSERT(core, !exists);
    
    // Test file removal
    err = core->file_remove(core, new_path);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File removal should succeed");
    
    err = core->file_exists(core, new_path, &exists);
    INFRAX_ASSERT_MSG(core, INFRAX_ERROR_IS_OK(err), "File exists check should succeed");
    INFRAX_ASSERT(core, !exists);
    
    printf("File operations tests passed!\n");
}

int main(void) {
    printf("Starting architecture tests...\n");
    infra = ppx_infra();
    //core = infra->core;//should be the same as InfraxCoreClass.singleton()
    //core = InfraxCoreClass.singleton();
    core = infra->core;
    InfraxError err = make_error(INFRAX_ERROR_OK, "OK");
    printf("test make_error %d,%s\n",err.code,err.message);
    test_infrax_core();
    test_string_operations();
    test_time_operations();
    test_random_operations();
    test_buffer_operations();
    test_ring_buffer_operations();
    test_file_operations();
    test_ppx_infra();
    
    printf("All tests passed!\n");
    return 0;
}
