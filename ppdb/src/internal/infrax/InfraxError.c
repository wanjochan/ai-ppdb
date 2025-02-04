#include "cosmopolitan.h"
#include "internal/infrax/InfraxError.h"

// Thread local storage for errors
static pthread_key_t error_key;
static pthread_once_t error_key_once = PTHREAD_ONCE_INIT;

static void destroy_error(void* data) {
    if (data) {
        infrax_error_free((InfraxError*)data);
    }
}

static void create_error_key(void) {
    pthread_key_create(&error_key, destroy_error);
}

// Initialize error instance
static void infrax_error_init(InfraxError* self) {
    if (!self) return;
    
    // Initialize state
    self->code = 0;
    self->message[0] = '\0';
    
    // Initialize methods
    self->new = infrax_error_new;
    self->free = infrax_error_free;
    self->set = infrax_error_set;
    self->clear = infrax_error_clear;
    self->get_message = infrax_error_get_message;
}

// Constructor
InfraxError* infrax_error_new(void) {
    InfraxError* self = (InfraxError*)malloc(sizeof(InfraxError));
    if (self) {
        infrax_error_init(self);
    }
    return self;
}

// Destructor
void infrax_error_free(InfraxError* self) {
    if (self) {
        free(self);
    }
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
    self->code = 0;
    self->message[0] = '\0';
}

const char* infrax_error_get_message(InfraxError* self) {
    return self ? self->message : "";
}

// Get thread-local error instance
InfraxError* get_global_infrax_error(void) {
    pthread_once(&error_key_once, create_error_key);
    
    InfraxError* error = pthread_getspecific(error_key);
    if (!error) {
        error = infrax_error_new();
        if (error) {
            pthread_setspecific(error_key, error);
        }
    }
    return error;
}
