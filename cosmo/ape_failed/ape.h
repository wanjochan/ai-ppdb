#ifndef COSMO_APE_H_
#define COSMO_APE_H_

#include "cosmopolitan.h"

// APE header structures
struct ape_header {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t entry;
    uint32_t text_start;
    uint32_t text_size;
    uint32_t data_start;
    uint32_t data_size;
    uint32_t bss_start;
    uint32_t bss_size;
    uint32_t dynsym_start;
    uint32_t dynsym_size;
    uint32_t dynstr_start;
    uint32_t dynstr_size;
    uint32_t hash_start;
    uint32_t hash_size;
    uint32_t got_start;
    uint32_t got_size;
    uint32_t rel_start;
    uint32_t rel_size;
    uint32_t init_start;
    uint32_t init_size;
    uint32_t fini_start;
    uint32_t fini_size;
};

// APE functions
int ape_add_pe_header(void* buf, size_t size);
int ape_add_pe_sections(void* buf, size_t size);
int ape_add_pe_exports(void* buf, size_t size);
int ape_set_dll_name(void* buf, size_t size, const char* name);

#endif // COSMO_APE_H_ 