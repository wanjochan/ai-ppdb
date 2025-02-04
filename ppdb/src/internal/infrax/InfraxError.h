#ifndef INFRAX_ERROR_H_
#define INFRAX_ERROR_H_

#include "cosmopolitan.h"

//注意，InfraxError 用值。不用指针

typedef int32_t infrax_error_t;

// Forward declaration
typedef struct InfraxError InfraxError;

// Error structure definition
struct InfraxError {
    // Error state
    infrax_error_t code;
    char message[128];
    // TODO: Add stack trace support when solution is available
    // void* stack_frames[32];
    // int stack_depth;
    
    // Error operations
    void (*set)(struct InfraxError* self, infrax_error_t code, const char* message);
    void (*clear)(struct InfraxError* self);
    const char* (*get_message)(const struct InfraxError* self);
};

// Error operations
void infrax_error_set(InfraxError* self, infrax_error_t code, const char* message);
void infrax_error_clear(InfraxError* self);
const char* infrax_error_get_message(const InfraxError* self);

// Create a new error instance
InfraxError infrax_error_create(infrax_error_t code, const char* message);

#endif // INFRAX_ERROR_H_
