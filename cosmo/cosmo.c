#include "cosmopolitan.h"

// Required by Cosmopolitan
void _apple(void) {}

// Forward declaration
int main(int argc, char *argv[]);

// Entry point
void _start(void) {
  // Call main with no arguments for now
  exit(main(0, NULL));
}

int main(int argc, char *argv[]) {
  printf("Cosmo loader starting...\n");

  // Load the module
  const char *module_path = "build/test.dbg";
  printf("Loading module: %s\n", module_path);

  // Open and map the module file
  int fd = open(module_path, O_RDONLY);
  if (fd < 0) {
    printf("Failed to open module file\n");
    return 1;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    printf("Failed to stat module file\n");
    close(fd);
    return 1;
  }

  printf("Module size: %ld bytes\n", st.st_size);

  // Map module into memory, let system choose address
  void *module = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (module == MAP_FAILED) {
    printf("Failed to map module file: %d\n", errno);
    close(fd);
    return 1;
  }

  printf("Module mapped at: %p\n", module);

  // Verify ELF header
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *)module;
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    printf("Invalid ELF magic\n");
    munmap(module, st.st_size);
    close(fd);
    return 1;
  }

  printf("ELF header verified\n");

  // Find the section headers
  Elf64_Shdr *shdr = (Elf64_Shdr *)((char *)module + ehdr->e_shoff);
  char *shstrtab = (char *)module + shdr[ehdr->e_shstrndx].sh_offset;

  // Find the symbol table and string table
  Elf64_Shdr *symtab = NULL;
  Elf64_Shdr *strtab = NULL;
  Elf64_Shdr *rela = NULL;
  Elf64_Shdr *text = NULL;
  void *module_main = NULL;

  for (int i = 0; i < ehdr->e_shnum; i++) {
    printf("Section %d: %s at offset 0x%lx, addr 0x%lx\n", 
           i, 
           shstrtab + shdr[i].sh_name,
           shdr[i].sh_offset,
           shdr[i].sh_addr);

    if (shdr[i].sh_type == SHT_SYMTAB) {
      symtab = &shdr[i];
    } else if (shdr[i].sh_type == SHT_STRTAB && 
               strcmp(shstrtab + shdr[i].sh_name, ".strtab") == 0) {
      strtab = &shdr[i];
    } else if (shdr[i].sh_type == SHT_RELA) {
      rela = &shdr[i];
    } else if (strcmp(shstrtab + shdr[i].sh_name, ".text") == 0) {
      text = &shdr[i];
    }

    // Find module_main section
    if (strcmp(shstrtab + shdr[i].sh_name, ".text.module_main") == 0) {
      module_main = (void *)((char *)module + shdr[i].sh_offset);
      printf("Found module_main at offset 0x%lx, addr 0x%lx\n", 
             shdr[i].sh_offset, shdr[i].sh_addr);
    }
  }

  if (!symtab || !strtab) {
    printf("Failed to find symbol tables\n");
    munmap(module, st.st_size);
    close(fd);
    return 1;
  }

  // Make text section executable
  if (text) {
    void *text_addr = (void *)((char *)module + text->sh_offset);
    size_t text_size = text->sh_size;
    if (mprotect(text_addr, text_size, PROT_READ | PROT_EXEC) < 0) {
      printf("Failed to make text section executable: %d\n", errno);
      munmap(module, st.st_size);
      close(fd);
      return 1;
    }
    printf("Made text section executable at %p, size %ld\n", text_addr, text_size);
  }

  // Process relocations if present
  if (rela) {
    printf("Processing relocations...\n");
    Elf64_Rela *rels = (Elf64_Rela *)((char *)module + rela->sh_offset);
    Elf64_Sym *syms = (Elf64_Sym *)((char *)module + symtab->sh_offset);
    char *strs = (char *)module + strtab->sh_offset;
    int nrels = rela->sh_size / sizeof(Elf64_Rela);

    for (int i = 0; i < nrels; i++) {
      Elf64_Rela *rel = &rels[i];
      Elf64_Sym *sym = &syms[ELF64_R_SYM(rel->r_info)];
      const char *name = strs + sym->st_name;

      printf("Relocation: offset=0x%lx type=%ld symbol=%s\n",
             rel->r_offset, ELF64_R_TYPE(rel->r_info), name);

      // Apply relocation
      uint64_t *target = (uint64_t *)((char *)module + rel->r_offset);
      uint64_t symbol_addr = (uint64_t)module + sym->st_value;

      switch (ELF64_R_TYPE(rel->r_info)) {
        case R_X86_64_RELATIVE:
          *target = (uint64_t)module + rel->r_addend;
          break;
        case R_X86_64_64:
          *target = symbol_addr + rel->r_addend;
          break;
        case R_X86_64_PC32:
          *target = (uint32_t)(symbol_addr + rel->r_addend - (uint64_t)target);
          break;
        case R_X86_64_PLT32:
          *target = (uint32_t)(symbol_addr + rel->r_addend - (uint64_t)target);
          break;
        default:
          printf("Unknown relocation type: %ld\n", ELF64_R_TYPE(rel->r_info));
          break;
      }
    }
  }

  if (!module_main) {
    printf("Failed to find module_main\n");
    munmap(module, st.st_size);
    close(fd);
    return 1;
  }

  // Call module_main
  printf("Calling module_main at %p...\n", module_main);
  int (*main_fn)(void) = module_main;
  int result = main_fn();

  printf("Module returned: %d\n", result);

  munmap(module, st.st_size);
  close(fd);
  return 0;
}
