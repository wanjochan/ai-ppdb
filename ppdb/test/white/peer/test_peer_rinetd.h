#ifndef TEST_PEER_RINETD_H
#define TEST_PEER_RINETD_H

#include "test/test_common.h"
#include "internal/peer/peer_rinetd.h"

// Test cases
void test_rinetd_init(void);
void test_rinetd_cleanup(void);
void test_rinetd_config(void);
void test_rinetd_rule(void);
void test_rinetd_forward(void);
void test_rinetd_service(void);

// Test plan
static const test_case_t test_rinetd_cases[] = {
    {"test_rinetd_init", test_rinetd_init},
    {"test_rinetd_cleanup", test_rinetd_cleanup},
    {"test_rinetd_config", test_rinetd_config},
    {"test_rinetd_rule", test_rinetd_rule},
    {"test_rinetd_forward", test_rinetd_forward},
    {"test_rinetd_service", test_rinetd_service},
    {NULL, NULL}  // End marker
};

#endif // TEST_PEER_RINETD_H 