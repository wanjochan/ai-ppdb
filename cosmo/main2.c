#include "runtime.h"

__declspec(dllexport)
int module_main(int argc, char* argv[]) {
    printf("Hello from main2.dll!\n");
    printf("Arguments:\n");
    for (int i = 0; i < argc; i++) {
        printf("  %d: %s\n", i, argv[i]);
    }
    return 0;
}

// DLL 入口点
__declspec(dllexport)
bool DllMain(void* hinstDLL, unsigned long fdwReason, void* lpvReserved) {
    switch (fdwReason) {
        case 1: // DLL_PROCESS_ATTACH
            break;
        case 0: // DLL_PROCESS_DETACH
            break;
        case 2: // DLL_THREAD_ATTACH
            break;
        case 3: // DLL_THREAD_DETACH
            break;
    }
    return true;
} 