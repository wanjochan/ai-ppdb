#include "cosmopolitan.h"

/* 插件版本和魔数 */
#define PLUGIN_VERSION 1
#define PLUGIN_MAGIC 0x50504442 /* "PPDB" */

/* 插件函数表结构 */
#pragma pack(push, 1)
struct plugin_interface {
    uint32_t magic;    /* 魔数，用于识别插件 */
    uint32_t version;  /* 版本号 */
    
    /* Core模块函数 */
    unsigned char core_init[16];    /* 初始化core模块 */
    unsigned char core_alloc[16];   /* 内存分配函数 */
    
    /* Net模块函数 */
    unsigned char net_connect[16];  /* 网络连接函数 */
    unsigned char net_send[16];     /* 数据发送函数 */
};
#pragma pack(pop)

/* 函数指针类型定义 */
typedef int (*core_init_t)(void);
typedef void* (*core_alloc_t)(size_t size);
typedef int (*net_connect_t)(void);
typedef int (*net_send_t)(void* data);

/* 查找插件段 */
static struct plugin_interface* find_plugin_section(void* base) {
    dprintf(1, "Finding plugin section...\n");
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        dprintf(2, "Invalid ELF magic\n");
        return NULL;
    }

    dprintf(1, "ELF header found, section count: %d\n", ehdr->e_shnum);
    Elf64_Shdr* shdr = (Elf64_Shdr*)((char*)base + ehdr->e_shoff);
    char* shstrtab = (char*)base + shdr[ehdr->e_shstrndx].sh_offset;
    
    Elf64_Shdr* plugin_shdr = NULL;
    
    // 查找.plugin段
    for (int i = 0; i < ehdr->e_shnum; i++) {
        char* name = shstrtab + shdr[i].sh_name;
        dprintf(1, "Section %d: %s\n", i, name);
        if (strcmp(name, ".plugin") == 0) {
            plugin_shdr = &shdr[i];
            dprintf(1, "Found .plugin section at offset 0x%lx\n", plugin_shdr->sh_offset);
            break;
        }
    }
    
    if (!plugin_shdr) {
        dprintf(2, "Failed to find .plugin section\n");
        return NULL;
    }
    
    struct plugin_interface* api = (struct plugin_interface*)((char*)base + plugin_shdr->sh_offset);
    dprintf(1, "Plugin API found at 0x%p\n", api);
    return api;
}

int main(void) {
    // 打开并映射插件文件
    int fd = open("test8.dl", O_RDONLY);
    if (fd < 0) {
        dprintf(2, "Failed to open test8.dl\n");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        dprintf(2, "Failed to stat test8.dl\n");
        close(fd);
        return 1;
    }

    dprintf(1, "Plugin file size: %ld bytes\n", st.st_size);
    void* base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        dprintf(2, "Failed to mmap test8.dl\n");
        close(fd);
        return 1;
    }

    dprintf(1, "Plugin mapped at 0x%p\n", base);
    close(fd);

    // 查找插件接口
    struct plugin_interface* api = find_plugin_section(base);
    if (!api) {
        dprintf(2, "Failed to find plugin interface\n");
        munmap(base, st.st_size);
        return 1;
    }

    // 检查插件接口
    if (api->magic != PLUGIN_MAGIC) {
        dprintf(2, "Invalid plugin magic: 0x%x\n", api->magic);
        munmap(base, st.st_size);
        return 1;
    }

    if (api->version != PLUGIN_VERSION) {
        dprintf(2, "Invalid plugin version: %d\n", api->version);
        munmap(base, st.st_size);
        return 1;
    }

    // 测试插件接口
    dprintf(1, "Plugin interface loaded successfully\n");
    dprintf(1, "Magic: 0x%x\n", api->magic);
    dprintf(1, "Version: %d\n", api->version);

    // 测试 core_init 函数
    dprintf(1, "Testing core_init...\n");
    core_init_t core_init = (core_init_t)api->core_init;
    int ret = core_init();
    dprintf(1, "core_init returned: %d\n", ret);

    // 测试 core_alloc 函数
    dprintf(1, "Testing core_alloc...\n");
    core_alloc_t core_alloc = (core_alloc_t)api->core_alloc;
    void* ptr = core_alloc(100);
    dprintf(1, "core_alloc returned: 0x%p\n", ptr);

    // 测试 net_connect 函数
    dprintf(1, "Testing net_connect...\n");
    net_connect_t net_connect = (net_connect_t)api->net_connect;
    ret = net_connect();
    dprintf(1, "net_connect returned: %d\n", ret);

    // 测试 net_send 函数
    dprintf(1, "Testing net_send...\n");
    net_send_t net_send = (net_send_t)api->net_send;
    ret = net_send(ptr);
    dprintf(1, "net_send returned: %d\n", ret);

    // 卸载插件
    dprintf(1, "Unloading plugin...\n");
    munmap(base, st.st_size);
    dprintf(1, "Plugin unloaded\n");
    return 0;
} 