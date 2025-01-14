#include "cosmopolitan.h"

/* 插件版本和魔数 */
#define PLUGIN_VERSION 1
#define PLUGIN_MAGIC 0x50504442 /* "PPDB" */

/* 错误码定义 */
#define ERR_SUCCESS 0
#define ERR_INVALID_PARAM -1
#define ERR_OUT_OF_MEMORY -2
#define ERR_NETWORK_ERROR -3

/* 插件函数表结构 */
#pragma pack(push, 1)
struct plugin_interface {
    uint32_t magic;    /* 魔数，用于识别插件 */
    uint32_t version;  /* 版本号 */
    
    /* Core模块函数 */
    int (*core_init)(void);         /* 初始化core模块 */
    void* (*core_alloc)(size_t);    /* 内存分配函数 */
    
    /* Net模块函数 */
    int (*net_connect)(void);       /* 网络连接函数 */
    int (*net_send)(void*);         /* 数据发送函数 */
};
#pragma pack(pop)

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
    int ret = api->core_init();
    if (ret != ERR_SUCCESS) {
        dprintf(2, "core_init failed with error %d\n", ret);
        munmap(base, st.st_size);
        return 1;
    }
    dprintf(1, "core_init succeeded\n");

    // 测试 core_alloc 函数
    dprintf(1, "Testing core_alloc...\n");
    void* ptr = api->core_alloc(100);
    if (!ptr) {
        dprintf(2, "core_alloc failed\n");
        munmap(base, st.st_size);
        return 1;
    }
    dprintf(1, "core_alloc returned: 0x%p\n", ptr);

    // 测试 net_connect 函数
    dprintf(1, "Testing net_connect...\n");
    ret = api->net_connect();
    if (ret != 42) {
        dprintf(2, "net_connect failed with error %d\n", ret);
        munmap(base, st.st_size);
        return 1;
    }
    dprintf(1, "net_connect succeeded\n");

    // 测试 net_send 函数
    dprintf(1, "Testing net_send...\n");
    ret = api->net_send(ptr);
    if (ret < 0) {
        dprintf(2, "net_send failed with error %d\n", ret);
        munmap(base, st.st_size);
        return 1;
    }
    dprintf(1, "net_send succeeded, offset: %d\n", ret);

    // 卸载插件
    dprintf(1, "Unloading plugin...\n");
    munmap(base, st.st_size);
    dprintf(1, "Plugin unloaded\n");
    return 0;
} 