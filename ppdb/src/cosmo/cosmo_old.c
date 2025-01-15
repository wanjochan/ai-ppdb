#include "cosmopolitan.h"

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
        dprintf(2, "Failed to open plugin: %s (errno=%d)\n", path, errno);
        return NULL;
    }

    /* 获取文件大小 */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        dprintf(2, "Failed to stat plugin (errno=%d)\n", errno);
        close(fd);
        return NULL;
    }
    *size = st.st_size;
    dprintf(1, "Plugin file size: %zu bytes\n", *size);

    /* 检查文件大小 */
    if (*size < sizeof(struct plugin_header)) {
        dprintf(2, "Plugin file too small: %zu bytes\n", *size);
        close(fd);
        return NULL;
    }

    /* 映射文件到内存 */
    void* base = mmap(NULL, st.st_size, 
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE, fd, 0);
    close(fd);

    if (base == MAP_FAILED) {
        dprintf(2, "Failed to mmap plugin (errno=%d)\n", errno);
        return NULL;
    }
    dprintf(1, "Plugin mapped at: %p\n", base);

    return base;
}

/* 验证插件 */
static bool verify_plugin(void* base, size_t size) {
    struct plugin_header* header = (struct plugin_header*)base;
    dprintf(1, "Verifying plugin header:\n");
    dprintf(1, "  Magic: 0x%x\n", header->magic);
    dprintf(1, "  Version: %d\n", header->version);
    dprintf(1, "  Init offset: 0x%x\n", header->init_offset);
    dprintf(1, "  Main offset: 0x%x\n", header->main_offset);
    dprintf(1, "  Fini offset: 0x%x\n", header->fini_offset);
    
    /* 检查魔数和版本 */
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

    /* 检查偏移量 */
    if (header->init_offset >= size ||
        header->main_offset >= size ||
        header->fini_offset >= size) {
        dprintf(2, "Invalid function offset(s)\n");
        return false;
    }

    /* 检查对齐 */
    if (header->init_offset & 7 ||
        header->main_offset & 7 ||
        header->fini_offset & 7) {
        dprintf(2, "Function offsets not aligned\n");
        return false;
    }

    return true;
}

/* 显示用法信息 */
static void show_usage(const char* prog_name) {
    dprintf(2, "Usage: %s <plugin.dl>\n", prog_name);
}

/* 执行插件函数 */
static int call_plugin_func(const char* name, int (*func)(void)) {
    if (!func) {
        dprintf(1, "Skipping %s (not defined)\n", name);
        return 0;
    }

    dprintf(1, "Calling %s at %p...\n", name, func);
    int ret = func();
    dprintf(1, "%s returned: %d\n", name, ret);
    return ret;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        show_usage(argv[0]);
        return 1;
    }

    size_t size;
    void* base = load_plugin(argv[1], &size);
    if (!base) {
        return 1;
    }

    if (!verify_plugin(base, size)) {
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
    
    /* 调用init */
    ret = call_plugin_func("init", init);
    if (ret != 0) {
        dprintf(2, "Plugin init failed: %d\n", ret);
        goto cleanup;
    }

    /* 调用main */
    ret = call_plugin_func("main", main_func);

    /* 调用fini */
    int fini_ret = call_plugin_func("fini", fini);
    if (fini_ret != 0) {
        dprintf(2, "Plugin cleanup failed: %d\n", fini_ret);
        if (ret == 0) ret = fini_ret;
    }

cleanup:
    dprintf(1, "Unloading plugin...\n");
    munmap(base, size);
    return ret;
} 