#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <string.h>

typedef int (*add_func)(int, int);
typedef const char* (*get_version_func)(void);

int main() {
    const char* lib_name = getenv("DYLD_LIBRARY_PATH") ? "./ape_dl_poc.dylib" : "./libape_test.so";
    printf("Testing library: %s\n", lib_name);

    // Open the library
    void* handle = dlopen(lib_name, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    printf("Library loaded successfully\n");

    // Get add function
    add_func add = (add_func)dlsym(handle, "add");
    if (!add) {
        fprintf(stderr, "dlsym 'add' failed: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    // Get version function
    get_version_func get_version = (get_version_func)dlsym(handle, "get_version");
    if (!get_version) {
        fprintf(stderr, "dlsym 'get_version' failed: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    // Test add function
    int result = add(5, 3);
    printf("add(5, 3) = %d\n", result);

    // Test version function
    const char* version = get_version();
    printf("Version: %s\n", version);

    // Close the library
    dlclose(handle);
    printf("Library closed\n");

    return 0;
} 