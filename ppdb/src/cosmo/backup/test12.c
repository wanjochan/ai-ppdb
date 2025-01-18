/* APE加载器测试 */
#include "cosmopolitan.h"
#include "ape_loader.h"

/* 包装函数 */
void* __wrap_ape_stack_round(void* p) {
    return p;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <ape_file>\n", argv[0]);
        return 1;
    }

    /* 加载APE文件 */
    void* handle = ape_load(argv[1]);
    if (!handle) {
        printf("Failed to load APE file: %s\n", argv[1]);
        return 1;
    }

    /* 获取入口函数 */
    int (*entry)(void) = ape_get_proc(handle, "main");
    if (!entry) {
        printf("Failed to get entry point\n");
        ape_unload(handle);
        return 1;
    }

    /* 执行入口函数 */
    int result = entry();
    printf("APE program returned: %d\n", result);

    /* 卸载APE文件 */
    ape_unload(handle);
    return 0;
} 