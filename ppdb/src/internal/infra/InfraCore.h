#ifndef INFRA_CORE_H
#define INFRA_CORE_H

// Forward declaration
typedef struct InfraCore InfraCore;

// Core structure definition
struct InfraCore {
    // Private data
    int data;

    // Public methods
    InfraCore* (*new)(void);     // constructor: infra_core_new()
    void (*free)(InfraCore *self);// destructor: infra_core_free()
    void (*print)(InfraCore *self);// print function (using libc printf)

    // Future extensions
    // int (*get_data)(const InfraCore *self);
    // void (*set_data)(InfraCore *self, int value);
};

// Constructor - creates and initializes a new instance
InfraCore* infra_core_new(void);

// Destructor - cleans up and frees the instance
void infra_core_free(InfraCore *core);

#endif // INFRA_CORE_H




