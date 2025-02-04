#ifndef PPDB_INFRAX_CORE_H
#define PPDB_INFRAX_CORE_H

// Forward declaration
typedef struct InfraxCore InfraxCore;

// Core structure definition
struct InfraxCore {
    // Private data
    int data;

    // Public methods
    struct InfraxCore* (*new)(void);     // constructor: infrax_core_new()
    void (*free)(struct InfraxCore *self);// destructor: infrax_core_free()
    void (*print)(struct InfraxCore *self);// print function (using libc printf)

    // Future extensions
    // int (*get_data)(const InfraxCore *self);
    // void (*set_data)(InfraxCore *self, int value);
};

// Constructor - creates and initializes a new instance
InfraxCore* infrax_core_new(void);

// Destructor - cleans up and frees the instance
void infrax_core_free(InfraxCore *self);

#endif // PPDB_INFRAX_CORE_H
