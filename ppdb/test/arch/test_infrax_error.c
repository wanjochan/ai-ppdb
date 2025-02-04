/*
 * test_infrax_error.c - Test cases for InfraxError module
 */

#include "internal/infrax/InfraxError.h"
#include "../white/framework/test_framework.h"

void test_error_string(void) {
    TEST_ASSERT_MSG(strcmp("Success", infrax_error_string(INFRAX_OK)) == 0,
                   "Expected 'Success' for INFRAX_OK");
    TEST_ASSERT_MSG(strcmp("Unknown error", infrax_error_string(INFRAX_ERROR_UNKNOWN)) == 0,
                   "Expected 'Unknown error' for INFRAX_ERROR_UNKNOWN");
    TEST_ASSERT_MSG(strcmp("No memory", infrax_error_string(INFRAX_ERROR_NO_MEMORY)) == 0,
                   "Expected 'No memory' for INFRAX_ERROR_NO_MEMORY");
    TEST_ASSERT_MSG(strcmp("Invalid parameter", infrax_error_string(INFRAX_ERROR_INVALID_PARAM)) == 0,
                   "Expected 'Invalid parameter' for INFRAX_ERROR_INVALID_PARAM");
    TEST_ASSERT_MSG(strcmp("Unknown error", infrax_error_string(-999)) == 0,
                   "Expected 'Unknown error' for invalid error code");
}

void test_expected_error(void) {
    infrax_error_t test_error = INFRAX_ERROR_IO;
    
    // Test initial state
    TEST_ASSERT_FALSE(infrax_is_expected_error(test_error));
    
    // Test setting expected error
    infrax_set_expected_error(test_error);
    TEST_ASSERT_TRUE(infrax_is_expected_error(test_error));
    TEST_ASSERT_FALSE(infrax_is_expected_error(INFRAX_ERROR_TIMEOUT)); // Different error
    
    // Test clearing expected error
    infrax_clear_expected_error();
    TEST_ASSERT_FALSE(infrax_is_expected_error(test_error));
}

void test_system_error_mapping(void) {
    // Test system error to infrax error
    TEST_ASSERT_EQUAL(INFRAX_OK, infrax_error_from_system(0));
    TEST_ASSERT_EQUAL(INFRAX_ERROR_NO_MEMORY, infrax_error_from_system(ENOMEM));
    TEST_ASSERT_EQUAL(INFRAX_ERROR_ALREADY_EXISTS, infrax_error_from_system(EEXIST));
    TEST_ASSERT_EQUAL(INFRAX_ERROR_SYSTEM, infrax_error_from_system(999)); // Unknown system error
    
    // Test infrax error to system error
    TEST_ASSERT_EQUAL(0, infrax_error_to_system(INFRAX_OK));
    TEST_ASSERT_EQUAL(ENOMEM, infrax_error_to_system(INFRAX_ERROR_NO_MEMORY));
    TEST_ASSERT_EQUAL(EEXIST, infrax_error_to_system(INFRAX_ERROR_ALREADY_EXISTS));
    TEST_ASSERT_EQUAL(EINVAL, infrax_error_to_system(INFRAX_ERROR_UNKNOWN)); // Unknown infrax error
}

int main(void) {
    TEST_BEGIN();
    
    RUN_TEST(test_error_string);
    RUN_TEST(test_expected_error);
    RUN_TEST(test_system_error_mapping);
    
    TEST_END();
    return 0;
}
