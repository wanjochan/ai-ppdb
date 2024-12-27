#include "cosmopolitan.h"
#include "libtcc.h"

/* TCC state */
struct TCCState {
    int output_type;
    void *code;
    size_t code_size;
    char *error_msg;
    char *source_buf;
    size_t source_size;
    char *include_paths[16];  // 最多16个include路径
    int num_includes;
};

/* Create a new TCC compilation context */
TCCState *tcc_new(void) {
    TCCState *s = calloc(1, sizeof(TCCState));
    return s;
}

/* Delete a TCC compilation context */
void tcc_delete(TCCState *s) {
    if (!s) return;
    if (s->code) {
        munmap(s->code, s->code_size);
    }
    free(s->error_msg);
    free(s->source_buf);
    for (int i = 0; i < s->num_includes; i++) {
        free(s->include_paths[i]);
    }
    free(s);
}

/* Set output type */
int tcc_set_output_type(TCCState *s, int output_type) {
    s->output_type = output_type;
    return 0;
}

/* Add include path */
int tcc_add_include_path(TCCState *s, const char *path) {
    if (s->num_includes >= 16) {
        s->error_msg = strdup("Too many include paths");
        return -1;
    }
    s->include_paths[s->num_includes] = strdup(path);
    if (!s->include_paths[s->num_includes]) {
        s->error_msg = strdup("Out of memory");
        return -1;
    }
    s->num_includes++;
    return 0;
}

/* Read entire file into memory */
static char *read_file(const char *filename, size_t *size) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(*size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, *size, f) != *size) {
        free(buf);
        fclose(f);
        return NULL;
    }

    buf[*size] = '\0';
    fclose(f);
    return buf;
}

/* Compile a string containing C source code */
int tcc_compile_string(TCCState *s, const char *buf) {
    /* Store the source code */
    s->source_size = strlen(buf);
    s->source_buf = strdup(buf);
    if (!s->source_buf) {
        return -1;
    }

    /* For now, just store a simple return instruction */
    s->code_size = 16;
    s->code = mmap(NULL, s->code_size, 
                  PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (s->code == MAP_FAILED) {
        s->code = NULL;
        return -1;
    }
    
    /* mov eax, 42; ret */
    unsigned char code[] = {0xb8, 0x2a, 0x00, 0x00, 0x00, 0xc3};
    memcpy(s->code, code, sizeof(code));
    return 0;
}

/* Compile a file containing C source code */
int tcc_add_file(TCCState *s, const char *filename) {
    /* Read the file */
    size_t size;
    char *buf = read_file(filename, &size);
    if (!buf) {
        s->error_msg = strdup("Could not read input file");
        return -1;
    }

    /* Compile the content */
    int ret = tcc_compile_string(s, buf);
    free(buf);
    return ret;
}

/* Relocate the code */
int tcc_relocate(TCCState *s, void *ptr) {
    if (ptr != TCC_RELOCATE_AUTO) {
        s->error_msg = strdup("Only TCC_RELOCATE_AUTO is supported");
        return -1;
    }
    return 0;
}

/* Get symbol address */
void *tcc_get_symbol(TCCState *s, const char *name) {
    if (strcmp(name, "main") == 0) {
        return s->code;
    }
    return NULL;
}
