#ifndef APE_LOADER_H
#define APE_LOADER_H

#include "cosmopolitan.h"
#include "ape/ape.h"
#include "libc/elf/elf.h"
#include "libc/runtime/runtime.h"

/* 基本定义 */
#define O_RDONLY    0
#define PROT_NONE   0
#define PROT_READ   1
#define PROT_WRITE  2
#define PROT_EXEC   4
#define MAP_SHARED  1
#define MAP_PRIVATE 2
#define MAP_FIXED   16

#define ELFCLASS32    1
#define ELFDATA2LSB   1
#define EM_NEXGEN32E  62
#define EM_AARCH64    183
#define ET_EXEC       2
#define ET_DYN        3
#define PT_LOAD       1
#define PT_DYNAMIC    2
#define PT_INTERP     3
#define EI_CLASS      4
#define EI_DATA       5
#define PF_X          1
#define PF_W          2
#define PF_R          4

#define EF_APE_MODERN      0x101ca75
#define EF_APE_MODERN_MASK 0x1ffffff

/* 结构定义 */
struct ElfEhdr {
    unsigned char e_ident[16];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned e_version;
    unsigned long e_entry;
    unsigned long e_phoff;
    unsigned long e_shoff;
    unsigned e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
};

struct ElfPhdr {
    unsigned p_type;
    unsigned p_flags;
    unsigned long p_offset;
    unsigned long p_vaddr;
    unsigned long p_paddr;
    unsigned long p_filesz;
    unsigned long p_memsz;
    unsigned long p_align;
};

union ElfEhdrBuf {
    struct ElfEhdr ehdr;
    char buf[4096];
};

struct ApeLoader {
    union {
        struct ElfPhdr phdr;
        char buf[4096];
    } phdr;
};

/* 函数声明 */
const char* TryElf(struct ApeLoader* M, union ElfEhdrBuf* ebuf,
                  char* exe, int fd, long* sp, long* auxv,
                  unsigned long pagesz, int os);

void Spawn(int os, char* exe, int fd, long* sp,
          unsigned long pagesz, struct ElfEhdr* e,
          struct ElfPhdr* p);

void* ape_load(const char* path);
void* ape_get_proc(void* handle, const char* symbol);
void ape_unload(void* handle);

#endif /* APE_LOADER_H */ 