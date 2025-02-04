#ifndef INFRAX_ERROR_H_
#define INFRAX_ERROR_H_

#include "cosmopolitan.h"

typedef int32_t infrax_error_t;

// Forward declaration
typedef struct InfraxError InfraxError;

// Error structure definition
struct InfraxError {
    // Error state
    infrax_error_t code;
    char message[256];
    // TODO: Add stack trace support when solution is available
    // void* stack_frames[32];
    // int stack_depth;
    
    // Methods
    struct InfraxError* (*new)(void);
    void (*free)(struct InfraxError* self);
    
    // Error operations
    void (*set)(struct InfraxError* self, infrax_error_t code, const char* message);
    void (*clear)(struct InfraxError* self);
    const char* (*get_message)(struct InfraxError* self);
};

// Constructor
InfraxError* infrax_error_new(void);

// Destructor
void infrax_error_free(InfraxError* self);

// Error operations
void infrax_error_set(InfraxError* self, infrax_error_t code, const char* message);
void infrax_error_clear(InfraxError* self);
const char* infrax_error_get_message(InfraxError* self);

// Get thread-local error instance
InfraxError* get_global_infrax_error(void);

#endif // INFRAX_ERROR_H_
