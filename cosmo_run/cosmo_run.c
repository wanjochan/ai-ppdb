#include "cosmopolitan.h"
#include "tinycc/libtcc.h"

/* Simple dynamic C code runner */
int main(int argc __attribute__((unused)), char **argv __attribute__((unused))) {
    TCCState *s;
    int ret;
    
    /* Create a new TCC compilation context */
    s = tcc_new();
    if (!s) {
        fprintf(stderr, "Could not create tcc context\n");
        return 1;
    }

    /* Add cosmo include path */
    ret = tcc_add_include_path(s, "../ppdb/cosmopolitan");
    if (ret < 0) {
        fprintf(stderr, "Could not add include path\n");
        tcc_delete(s);
        return 1;
    }

    /* Set output type to memory (JIT) */
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    /* Add a dummy main if no file is specified */
    if (argc < 2) {
        ret = tcc_compile_string(s, "int main() { return 42; }");
    } else {
        /* Compile the input file */
        ret = tcc_add_file(s, argv[1]);
    }

    if (ret == -1) {
        fprintf(stderr, "Compilation failed\n");
        return 1;
    }

    /* Relocate the code */
    if (tcc_relocate(s) < 0) {
        fprintf(stderr, "Relocation failed\n");
        return 1;
    }

    /* Get entry point */
    int (*func)(void);
    func = (int (*)(void))tcc_get_symbol(s, "main");
    if (!func) {
        fprintf(stderr, "Could not find main()\n");
        return 1;
    }

    /* Run the code */
    ret = func();
    printf("Program returned %d\n", ret);

    /* Delete the context */
    tcc_delete(s);

    return 0;
}
