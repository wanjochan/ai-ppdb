#ifndef PPDB_INFRAX_CORE_H
#define PPDB_INFRAX_CORE_H

#include "internal/infrax/InfraxTypes.h"

// Forward declaration
typedef struct InfraxCore InfraxCore;

// Core structure definition
struct InfraxCore {
    // Private data
    int data;

    // Public methods
    struct InfraxCore* (*new)(void);     // constructor: infrax_core_new()
    void (*free)(struct InfraxCore *self);// destructor: infrax_core_free()
    // void (*print)(struct InfraxCore *self);// print function (using libc printf)
    
    // Printf forwarding
    int (*printf)(const char* format, ...);

    // Future extensions
    // int (*get_data)(const InfraxCore *self);
    // void (*set_data)(InfraxCore *self, int value);
    
    // Parameter forwarding function
    void* (*forward_call)(void* (*target_func)(), ...);
};

// Constructor - creates and initializes a new instance
InfraxCore* infrax_core_new(void);

// Destructor - cleans up and frees the instance
void infrax_core_free(InfraxCore *self);

// Printf forwarding function
int infrax_core_printf(const char* format, ...);

// Parameter forwarding function
void* infrax_core_forward_call(void* (*target_func)(va_list), ...);

InfraxCore* get_global_infra_core(void);

#endif // PPDB_INFRAX_CORE_H
