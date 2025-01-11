#ifndef PPDB_TEST_INFRA_MOCK_MEMORY_H
#define PPDB_TEST_INFRA_MOCK_MEMORY_H

#include "framework/mock_framework/mock_framework.h"
#include "internal/infra/infra.h"

// Memory allocation mocks
MOCK_FUNC(void*, infra_malloc, size_t size);
MOCK_FUNC(void, infra_free, void* ptr);

// Memory operation mocks
MOCK_FUNC(void*, infra_memset, void* s, int c, size_t n);
MOCK_FUNC(void*, infra_memcpy, void* dest, const void* src, size_t n);
MOCK_FUNC(void*, infra_memmove, void* dest, const void* src, size_t n);

// Mock expectation getters
mock_expectation_t* mock_expect_infra_malloc(void);
mock_expectation_t* mock_expect_infra_free(void);
mock_expectation_t* mock_expect_infra_memset(void);
mock_expectation_t* mock_expect_infra_memcpy(void);
mock_expectation_t* mock_expect_infra_memmove(void);

// Initialize memory mocks
mock_error_t mock_memory_init(void);

// Cleanup memory mocks
void mock_memory_cleanup(void);

#endif // PPDB_TEST_INFRA_MOCK_MEMORY_H