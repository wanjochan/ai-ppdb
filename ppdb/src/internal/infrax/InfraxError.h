/*
 * InfraxError.h - Error handling support for Infrax
 */
#ifndef INFRAX_ERROR_H_
#define INFRAX_ERROR_H_

#include "cosmopolitan.h"

//-----------------------------------------------------------------------------
// Error Type and Structure
//-----------------------------------------------------------------------------

typedef enum {
    INFRAX_OK = 0,
    INFRAX_ERROR_UNKNOWN = -1,
    INFRAX_ERROR_INVALID = -2,
    INFRAX_ERROR_INVALID_PARAM = -3,
    INFRAX_ERROR_NO_MEMORY = -4,
    INFRAX_ERROR_EXISTS = -5,
    INFRAX_ERROR_NOT_READY = -6,
    INFRAX_ERROR_IO = -7,
    INFRAX_ERROR_TIMEOUT = -8,
    INFRAX_ERROR_BUSY = -9,
    INFRAX_ERROR_DEPENDENCY = -10,
    INFRAX_ERROR_NOT_FOUND = -11,
    INFRAX_ERROR_SYSTEM = -12,
    INFRAX_ERROR_WOULD_BLOCK = -13,
    INFRAX_ERROR_CLOSED = -14,
    INFRAX_ERROR_NOT_SUPPORTED = -15,
    INFRAX_ERROR_ALREADY_EXISTS = -16,
    INFRAX_ERROR_INVALID_OPERATION = -17,
    INFRAX_ERROR_RUNTIME = -18,
    INFRAX_ERROR_INVALID_STATE = -19,
    INFRAX_ERROR_INVALID_CONFIG = -20,
    INFRAX_ERROR_CAS_MISMATCH = -21,
    INFRAX_ERROR_INVALID_TYPE = -22,
    INFRAX_ERROR_PROTOCOL = -23,
    INFRAX_ERROR_CONNECT_FAILED = -24,
    INFRAX_ERROR_NO_SPACE = -25,
    INFRAX_ERROR_INVALID_FORMAT = -26,
    INFRAX_ERROR_NOT_INITIALIZED = -27,
    INFRAX_ERROR_QUERY_FAILED = -28,
    INFRAX_ERROR_INVALID_URL = -29,
    INFRAX_ERROR_OPEN_FAILED = -30,
    INFRAX_ERROR_ALREADY_INITIALIZED = -31,
    INFRAX_ERROR_INIT_FAILED = -32,
    INFRAX_ERROR_LOCK_FAILED = -33,
    INFRAX_ERROR_UNLOCK_FAILED = -34,
    INFRAX_ERROR_WAIT_FAILED = -35,
    INFRAX_ERROR_SIGNAL_FAILED = -36,
    INFRAX_ERROR_DESTROY_FAILED = -37,
    INFRAX_ERROR_THREAD_CREATE = -38,
    INFRAX_ERROR_THREAD_JOIN = -39,
    INFRAX_ERROR_THREAD_DETACH = -40,
    INFRAX_ERROR_SHUTDOWN = -41,
    INFRAX_ERROR_MAX
} infrax_error_t;

typedef struct InfraxError {
    infrax_error_t last_error;
} InfraxError;

//-----------------------------------------------------------------------------
// Global Instance
//-----------------------------------------------------------------------------

InfraxError* get_global_infrax_error(void);

//-----------------------------------------------------------------------------
// Error Handling Functions
//-----------------------------------------------------------------------------

const char* infrax_error_string(infrax_error_t err);
void infrax_set_expected_error(infrax_error_t err);
void infrax_clear_expected_error(void);
bool infrax_is_expected_error(infrax_error_t err);
infrax_error_t infrax_error_from_system(int system_error);
int infrax_error_to_system(infrax_error_t error);

// Constructor and destructor
InfraxError* infrax_error_new(void);
void infrax_error_free(InfraxError* self);

#endif // INFRAX_ERROR_H_
