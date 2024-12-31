// Basic types
typedef unsigned long size_t;

// Windows API declarations
__attribute__((ms_abi)) int WriteConsoleA(void* hConsoleOutput, const char* lpBuffer, unsigned long nNumberOfCharsToWrite, unsigned long* lpNumberOfCharsWritten, void* lpReserved);
__attribute__((ms_abi)) void* GetStdHandle(unsigned long nStdHandle);

#define STD_OUTPUT_HANDLE ((unsigned long)-11)

// Calculate string length
static size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

// Print string to stdout
int puts(const char *s) {
    void* handle = GetStdHandle(STD_OUTPUT_HANDLE);
    unsigned long written;
    size_t len = strlen(s);
    
    if (WriteConsoleA(handle, s, len, &written, 0)) {
        WriteConsoleA(handle, "\n", 1, &written, 0);
        return 0;
    }
    return -1;
} 