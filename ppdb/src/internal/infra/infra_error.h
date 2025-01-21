/*
 * infra_error.h - Error handling support
 */
#ifndef INFRA_ERROR_H_
#define INFRA_ERROR_H_

#include "cosmopolitan.h"

//-----------------------------------------------------------------------------
// Error Type
//-----------------------------------------------------------------------------

typedef enum {
    INFRA_OK = 0,
    INFRA_ERROR_INVALID = -1, // EMPTY/NULL 
    INFRA_ERROR_INVALID_PARAM = -2, //@infra_init() and infra_config_init()
    INFRA_ERROR_NO_MEMORY = -3,
    INFRA_ERROR_EXISTS = -4,
    INFRA_ERROR_NOT_READY = -5,
    INFRA_ERROR_IO = -6,
    INFRA_ERROR_TIMEOUT = -7,
    INFRA_ERROR_BUSY = -8,
    INFRA_ERROR_DEPENDENCY = -9,
    INFRA_ERROR_NOT_FOUND = -10,
    INFRA_ERROR_SYSTEM = -11,
    INFRA_ERROR_WOULD_BLOCK = -12,
    INFRA_ERROR_CLOSED = -13,
    INFRA_ERROR_NOT_SUPPORTED = -14,
    INFRA_ERROR_ALREADY_EXISTS = -15,
    INFRA_ERROR_INVALID_OPERATION = -16,  // Invalid operation for current state
    INFRA_ERROR_RUNTIME = -17,  // 运行时错误
    INFRA_ERROR_MAX
} infra_error_t;

//-----------------------------------------------------------------------------
// Error Handling Functions
//-----------------------------------------------------------------------------

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

#endif // INFRA_ERROR_H_ 
