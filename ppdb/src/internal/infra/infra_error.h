/*
 * infra_error.h - Error handling support
 */
#ifndef INFRA_ERROR_H_
#define INFRA_ERROR_H_

#include "infra_core.h"

// Error codes
#define INFRA_ERROR_INVALID    -1  // Invalid parameter
#define INFRA_ERROR_MEMORY     -2  // Memory error
#define INFRA_ERROR_TIMEOUT    -3  // Timeout
#define INFRA_ERROR_BUSY       -4  // Resource busy
#define INFRA_ERROR_NOT_FOUND  -5  // Not found
#define INFRA_ERROR_EXISTS     -6  // Already exists
#define INFRA_ERROR_IO         -7  // I/O error

// Error handling functions
const char* infra_error_string(infra_error_t err);

// 预期错误处理函数
void infra_set_expected_error(infra_error_t err);
void infra_clear_expected_error(void);
bool infra_is_expected_error(infra_error_t err);

//-----------------------------------------------------------------------------
// System Error Code Mapping
//-----------------------------------------------------------------------------

infra_error_t infra_error_from_system(int system_error);
int infra_error_to_system(infra_error_t error);

//-----------------------------------------------------------------------------
// Error String Operations
//-----------------------------------------------------------------------------

#endif // INFRA_ERROR_H_ 