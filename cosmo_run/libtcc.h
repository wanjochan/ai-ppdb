#ifndef LIBTCC_H
#define LIBTCC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque state structure */
typedef struct TCCState TCCState;

/* Output type */
#define TCC_OUTPUT_MEMORY 1

/* Relocate options */
#define TCC_RELOCATE_AUTO (void*)1

/* Create a new TCC compilation context */
TCCState *tcc_new(void);

/* Delete a TCC compilation context */
void tcc_delete(TCCState *s);

/* Set output type */
int tcc_set_output_type(TCCState *s, int output_type);

/* Add include path */
int tcc_add_include_path(TCCState *s, const char *path);

/* Compile a string containing C source code */
int tcc_compile_string(TCCState *s, const char *buf);

/* Compile a file containing C source code */
int tcc_add_file(TCCState *s, const char *filename);

/* Relocate the code */
int tcc_relocate(TCCState *s, void *ptr);

/* Get symbol address */
void *tcc_get_symbol(TCCState *s, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* LIBTCC_H */
