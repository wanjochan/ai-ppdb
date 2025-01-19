#include "cosmopolitan.h"

//EXTERN_C
__attribute__((__noreturn__)) void ApeLoader(long di, long *sp, char dl);


#define READ32(S)                                                      \
  ((unsigned)(255 & (S)[3]) << 030 | (unsigned)(255 & (S)[2]) << 020 | \
   (unsigned)(255 & (S)[1]) << 010 | (unsigned)(255 & (S)[0]) << 000)

int load_and_run_ape(const char* filename) {
    if (IsWindows()) {
        printf("DEBUG: Trying to open file: %s\n", filename);
        
        // Open file
        int fd = open(filename, O_RDONLY);
        printf("DEBUG: Open result fd=%d\n", fd);
        
        if (fd < 0) {
            printf("Failed to open file %s\n", filename);
            return 1;
        }

        // Get file size
        struct stat st;
        if (fstat(fd, &st) < 0) {
            close(fd);
            return 1;
        }

        // Map file into memory with all permissions
        void* base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) {
            close(fd);
            printf("Failed to mmap: %d\n", errno);
            return 1;
        }

        // Read file into memory
        if (read(fd, base, st.st_size) != st.st_size) {
            munmap(base, st.st_size);
            close(fd);
            printf("Failed to read file\n");
            return 1;
        }

        // Get PE header offset and entry point
        uint32_t peOffset = READ32((uint8_t*)base + 0x3c);
        printf("DEBUG: PE offset: 0x%x\n", peOffset);
        uint32_t entryPoint = READ32((uint8_t*)base + peOffset + 0x28);
        printf("DEBUG: Entry point: 0x%x\n", entryPoint);
        void* entry = (uint8_t*)base + entryPoint;

        printf("DEBUG: Executing entry point at %p\n", entry);
        
        // Call entry point
        typedef int (*EntryPoint)(void*, void*, char*, int);
        EntryPoint WinMain = (EntryPoint)entry;
        int result = WinMain(base, NULL, "", 0);

        // Cleanup
        munmap(base, st.st_size);
        close(fd);
        return result;
    } else {
        // Unix loading via ApeLoader
        long args_with_count[] = {
            2,                    
            (long)"test_target.exe",         
            (long)"test_target.exe", 
            0,                   
            0                    
        };
        ApeLoader(0, args_with_count, 0);
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <target>\n", argv[0]);
        return 1;
    }
    return load_and_run_ape(argv[1]);
}
