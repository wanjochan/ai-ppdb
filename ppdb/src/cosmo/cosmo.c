#include "cosmopolitan.h"

/* 包装函数 */
void* ape_stack_round(void* p) {
    return p;
}

/* 插件头部魔数和版本 */
#define PLUGIN_MAGIC 0x50504442
#define PLUGIN_VERSION 1

/* 插件头部结构 */
struct plugin_header {
    uint32_t magic;
    uint32_t version;
    uint32_t init_offset;
    uint32_t main_offset;
    uint32_t fini_offset;
};

/* 加载插件 */
static void* load_plugin(const char* path, size_t* size) {
    printf("Loading plugin: %s\n", path);
    
    /* 打开插件文件 */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Failed to open plugin: %s\n", path);
        return NULL;
    }

    /* 获取文件大小 */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("Failed to stat plugin\n");
        close(fd);
        return NULL;
    }
    *size = st.st_size;
    printf("Plugin file size: %zu bytes\n", *size);

    /* 映射文件到内存 */
    void* base = mmap(NULL, st.st_size, 
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);

    if (base == MAP_FAILED) {
        printf("Failed to mmap plugin\n");
        return NULL;
    }
    printf("Plugin mapped at: %p\n", base);

    return base;
}

/* 验证插件 */
static bool verify_plugin(void* base) {
    struct plugin_header* header = (struct plugin_header*)base;
    printf("Verifying plugin header:\n");
    printf("  Magic: 0x%x\n", header->magic);
    printf("  Version: %d\n", header->version);
    printf("  Init offset: 0x%x\n", header->init_offset);
    printf("  Main offset: 0x%x\n", header->main_offset);
    printf("  Fini offset: 0x%x\n", header->fini_offset);
    
    if (header->magic != PLUGIN_MAGIC) {
        printf("Invalid plugin magic: expected 0x%x, got 0x%x\n",
               PLUGIN_MAGIC, header->magic);
        return false;
    }

    if (header->version != PLUGIN_VERSION) {
        printf("Invalid plugin version: expected %d, got %d\n",
               PLUGIN_VERSION, header->version);
        return false;
    }

    return true;
}

int main(void) {
    /* 初始化stdio */
    setvbuf(stdout, NULL, _IONBF, 0);

    size_t size;
    void* base = load_plugin("test11.dl", &size);//todo get from cmdline the first arg, and the second arg is dl_main
    if (!base) {
        return 1;
    }

    if (!verify_plugin(base)) {
        munmap(base, size);
        return 1;
    }

    struct plugin_header* header = (struct plugin_header*)base;
    
    /* 获取函数指针 */
    int (*init)(void) = (int (*)(void))((char*)base + header->init_offset);
    int (*main_func)(void) = (int (*)(void))((char*)base + header->main_offset);
    int (*fini)(void) = (int (*)(void))((char*)base + header->fini_offset);

    printf("Function addresses:\n");
    printf("  init: %p (offset: 0x%x)\n", init, header->init_offset);
    printf("  main: %p (offset: 0x%x)\n", main_func, header->main_offset);
    printf("  fini: %p (offset: 0x%x)\n", fini, header->fini_offset);

    /* 执行插件 */
    int ret = 0;
    if (init) {
        printf("Calling init...\n");
        ret = init();
        if (ret != 0) {
            printf("Plugin init failed: %d\n", ret);
            goto cleanup;
        }
        printf("Init returned: %d\n", ret);
    }

    if (main_func) {
        printf("Calling main...\n");
        ret = main_func();
        printf("Main returned: %d\n", ret);
    }

    if (fini) {
        printf("Calling fini...\n");
        ret = fini();
        printf("Fini returned: %d\n", ret);
    }

cleanup:
    munmap(base, size);
    return ret;
} 