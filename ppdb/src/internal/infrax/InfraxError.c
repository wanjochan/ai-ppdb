#include "cosmopolitan.h"
#include "internal/infrax/InfraxError.h"

// Initialize error instance
static void infrax_error_init(InfraxError* self) {
    if (!self) return;
    
    // Initialize state
    self->code = 0;
    self->message[0] = '\0';
    
    // Initialize methods
    self->set = infrax_error_set;
    self->clear = infrax_error_clear;
    self->get_message = infrax_error_get_message;
}

// Create a new error instance
InfraxError infrax_error_create(infrax_error_t code, const char* message) {
    InfraxError self;
    infrax_error_init(&self);
    infrax_error_set(&self, code, message);
    return self;
}

// Error operations
void infrax_error_set(InfraxError* self, infrax_error_t code, const char* message) {
    if (!self) return;
    
    self->code = code;
    if (message) {
        strncpy(self->message, message, sizeof(self->message) - 1);
        self->message[sizeof(self->message) - 1] = '\0';
    } else {
        self->message[0] = '\0';
    }
}

void infrax_error_clear(InfraxError* self) {
    if (!self) return;
    infrax_error_set(self, 0, "");
}

const char* infrax_error_get_message(const InfraxError* self) {
    return self ? self->message : "";
}
