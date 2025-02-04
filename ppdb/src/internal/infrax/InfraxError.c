/*
 * InfraxError.c - Error handling implementation for Infrax
 */

#include "internal/infrax/InfraxError.h"

// Global instance
static InfraxError* g_infrax_error = NULL;

// Expected error state
static struct {
    bool has_expected;
    infrax_error_t expected_error;
} g_error_state = {false, INFRAX_OK};

const char* infrax_error_string(infrax_error_t err) {
    switch (err) {
        case INFRAX_OK:
            return "Success";
        case INFRAX_ERROR_UNKNOWN:
            return "Unknown error";
        case INFRAX_ERROR_INVALID:
            return "Invalid parameter";
        case INFRAX_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case INFRAX_ERROR_NO_MEMORY:
            return "No memory";
        case INFRAX_ERROR_EXISTS:
            return "Already exists";
        case INFRAX_ERROR_NOT_READY:
            return "Not ready";
        case INFRAX_ERROR_IO:
            return "I/O error";
        case INFRAX_ERROR_TIMEOUT:
            return "Timeout";
        case INFRAX_ERROR_BUSY:
            return "Resource busy";
        case INFRAX_ERROR_DEPENDENCY:
            return "Dependency error";
        case INFRAX_ERROR_NOT_FOUND:
            return "Not found";
        case INFRAX_ERROR_SYSTEM:
            return "System error";
        case INFRAX_ERROR_WOULD_BLOCK:
            return "Operation would block";
        case INFRAX_ERROR_CLOSED:
            return "Resource closed";
        case INFRAX_ERROR_NOT_SUPPORTED:
            return "Not supported";
        case INFRAX_ERROR_ALREADY_EXISTS:
            return "Already exists";
        case INFRAX_ERROR_INVALID_OPERATION:
            return "Invalid operation";
        case INFRAX_ERROR_RUNTIME:
            return "Runtime error";
        case INFRAX_ERROR_INVALID_STATE:
            return "Invalid state";
        case INFRAX_ERROR_INVALID_CONFIG:
            return "Invalid configuration";
        case INFRAX_ERROR_CAS_MISMATCH:
            return "CAS mismatch";
        case INFRAX_ERROR_INVALID_TYPE:
            return "Invalid type";
        case INFRAX_ERROR_PROTOCOL:
            return "Protocol error";
        case INFRAX_ERROR_CONNECT_FAILED:
            return "Connection failed";
        case INFRAX_ERROR_NO_SPACE:
            return "No space available";
        case INFRAX_ERROR_INVALID_FORMAT:
            return "Invalid format";
        case INFRAX_ERROR_NOT_INITIALIZED:
            return "Not initialized";
        case INFRAX_ERROR_QUERY_FAILED:
            return "Query failed";
        case INFRAX_ERROR_INVALID_URL:
            return "Invalid URL";
        case INFRAX_ERROR_OPEN_FAILED:
            return "Open failed";
        case INFRAX_ERROR_ALREADY_INITIALIZED:
            return "Already initialized";
        case INFRAX_ERROR_INIT_FAILED:
            return "Initialization failed";
        case INFRAX_ERROR_LOCK_FAILED:
            return "Lock failed";
        case INFRAX_ERROR_UNLOCK_FAILED:
            return "Unlock failed";
        case INFRAX_ERROR_WAIT_FAILED:
            return "Wait failed";
        case INFRAX_ERROR_SIGNAL_FAILED:
            return "Signal failed";
        case INFRAX_ERROR_DESTROY_FAILED:
            return "Destroy failed";
        case INFRAX_ERROR_THREAD_CREATE:
            return "Thread creation failed";
        case INFRAX_ERROR_THREAD_JOIN:
            return "Thread join failed";
        case INFRAX_ERROR_THREAD_DETACH:
            return "Thread detach failed";
        case INFRAX_ERROR_SHUTDOWN:
            return "System shutdown";
        default:
            return "Unknown error";
    }
}

void infrax_set_expected_error(infrax_error_t err) {
    g_error_state.has_expected = true;
    g_error_state.expected_error = err;
}

void infrax_clear_expected_error(void) {
    g_error_state.has_expected = false;
    g_error_state.expected_error = INFRAX_OK;
}

bool infrax_is_expected_error(infrax_error_t err) {
    return g_error_state.has_expected && g_error_state.expected_error == err;
}

infrax_error_t infrax_error_from_system(int system_error) {
    if (system_error == 0) {
        return INFRAX_OK;
    }
    
    switch (system_error) {
        case ENOMEM:
            return INFRAX_ERROR_NO_MEMORY;
        case EEXIST:
            return INFRAX_ERROR_ALREADY_EXISTS;
        case ENOENT:
            return INFRAX_ERROR_NOT_FOUND;
        case EINVAL:
            return INFRAX_ERROR_INVALID_PARAM;
        case EBUSY:
            return INFRAX_ERROR_BUSY;
        case EIO:
            return INFRAX_ERROR_IO;
        case ETIMEDOUT:
            return INFRAX_ERROR_TIMEOUT;
        case EWOULDBLOCK:
            return INFRAX_ERROR_WOULD_BLOCK;
        case ENOTSUP:
            return INFRAX_ERROR_NOT_SUPPORTED;
        default:
            return INFRAX_ERROR_SYSTEM;
    }
}

int infrax_error_to_system(infrax_error_t error) {
    switch (error) {
        case INFRAX_OK:
            return 0;
        case INFRAX_ERROR_NO_MEMORY:
            return ENOMEM;
        case INFRAX_ERROR_ALREADY_EXISTS:
            return EEXIST;
        case INFRAX_ERROR_NOT_FOUND:
            return ENOENT;
        case INFRAX_ERROR_INVALID_PARAM:
            return EINVAL;
        case INFRAX_ERROR_BUSY:
            return EBUSY;
        case INFRAX_ERROR_IO:
            return EIO;
        case INFRAX_ERROR_TIMEOUT:
            return ETIMEDOUT;
        case INFRAX_ERROR_WOULD_BLOCK:
            return EWOULDBLOCK;
        case INFRAX_ERROR_NOT_SUPPORTED:
            return ENOTSUP;
        default:
            return EINVAL;
    }
}

InfraxError* infrax_error_new(void) {
    InfraxError* self = (InfraxError*)malloc(sizeof(InfraxError));
    if (self) {
        self->last_error = INFRAX_OK;
    }
    return self;
}

void infrax_error_free(InfraxError* self) {
    if (self) {
        free(self);
    }
}

InfraxError* get_global_infrax_error(void) {
    if (!g_infrax_error) {
        g_infrax_error = infrax_error_new();
    }
    return g_infrax_error;
}
