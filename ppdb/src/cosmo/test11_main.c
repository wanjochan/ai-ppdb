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
    dprintf(1, "Loading plugin: %s\n", path);
    
    /* 打开插件文件 */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        dprintf(2, "Failed to open plugin: %s\n", path);
        return NULL;
    }

    /* 获取文件大小 */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        dprintf(2, "Failed to stat plugin\n");
        close(fd);
        return NULL;
    }
    *size = st.st_size;
    dprintf(1, "Plugin file size: %zu bytes\n", *size);

    /* 映射文件到内存 */
    void* base = mmap(NULL, st.st_size, 
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);

    if (base == MAP_FAILED) {
        dprintf(2, "Failed to mmap plugin\n");
        return NULL;
    }
    dprintf(1, "Plugin mapped at: %p\n", base);

    return base;
}

/* 验证插件 */
static bool verify_plugin(void* base) {
    struct plugin_header* header = (struct plugin_header*)base;
    dprintf(1, "Verifying plugin header:\n");
    dprintf(1, "  Magic: 0x%x\n", header->magic);
    dprintf(1, "  Version: %d\n", header->version);
    dprintf(1, "  Init offset: 0x%x\n", header->init_offset);
    dprintf(1, "  Main offset: 0x%x\n", header->main_offset);
    dprintf(1, "  Fini offset: 0x%x\n", header->fini_offset);
    
    if (header->magic != PLUGIN_MAGIC) {
        dprintf(2, "Invalid plugin magic: expected 0x%x, got 0x%x\n",
               PLUGIN_MAGIC, header->magic);
        return false;
    }

    if (header->version != PLUGIN_VERSION) {
        dprintf(2, "Invalid plugin version: expected %d, got %d\n",
               PLUGIN_VERSION, header->version);
        return false;
    }

    return true;
}

int main(void) {
    size_t size;
    void* base = load_plugin("test11.dl", &size);
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

    dprintf(1, "Function addresses:\n");
    dprintf(1, "  init: %p (offset: 0x%x)\n", init, header->init_offset);
    dprintf(1, "  main: %p (offset: 0x%x)\n", main_func, header->main_offset);
    dprintf(1, "  fini: %p (offset: 0x%x)\n", fini, header->fini_offset);

    /* 执行插件 */
    int ret = 0;
    if (init) {
        dprintf(1, "Calling init...\n");
        ret = init();
        if (ret != 0) {
            dprintf(2, "Plugin init failed: %d\n", ret);
            goto cleanup;
        }
        dprintf(1, "Init returned: %d\n", ret);
    }

    if (main_func) {
        dprintf(1, "Calling main...\n");
        ret = main_func();
        dprintf(1, "Main returned: %d\n", ret);
    }

    if (fini) {
        dprintf(1, "Calling fini...\n");
        ret = fini();
        dprintf(1, "Fini returned: %d\n", ret);
    }

cleanup:
    munmap(base, size);
    return ret;
} 