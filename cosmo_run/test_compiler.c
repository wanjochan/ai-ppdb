#include "dynamic_compiler.h"
#include <stdio.h>

/* Test external function */
int add(int a, int b) {
    return a + b;
}

int main() {
    /* Create compiler instance */
    DynamicCompiler *dc = dc_create();
    if (!dc) {
        printf("Failed to create compiler\n");
        return 1;
    }

    /* Set compiler options */
    DCompilerOptions options = {
        .optimization_level = 2,
        .debug_info = true,
        .include_path = "."
    };
    dc_set_options(dc, &options);

    /* Add external function symbol */
    if (dc_add_symbol(dc, "add", (void*)add) != 0) {
        printf("Failed to add symbol\n");
        dc_destroy(dc);
        return 1;
    }

    /* Test compiling code that calls external function */
    const char *test_code = 
        "int main() {\n"
        "    extern int add(int a, int b);\n"
        "    return add(40, 2);\n"
        "}\n";

    if (dc_compile(dc, test_code) != 0) {
        printf("Compilation failed: %s\n", dc_get_error(dc));
        dc_destroy(dc);
        return 1;
    }

    /* Execute compiled code */
    int result = dc_execute(dc);
    printf("Execution result: %d\n", result);

    /* Cleanup */
    dc_destroy(dc);
    return 0;
}
