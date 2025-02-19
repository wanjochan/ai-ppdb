#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* 插件版本和魔数 */
#define PLUGIN_VERSION 1
#define PLUGIN_MAGIC 0x50504442 /* "PPDB" */

/* 插件函数表结构 */
#pragma pack(push, 1)
struct plugin_interface {
    uint32_t magic;    /* 魔数，用于识别插件 */
    uint32_t version;  /* 版本号 */
    unsigned char code[16];  /* 函数代码 */
};
#pragma pack(pop)

/* 加载插件文件 */
static struct plugin_interface* load_plugin(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        dprintf(2, "Failed to open %s\n", path);
        return NULL;
    }

    /* 获取文件大小 */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        dprintf(2, "Failed to stat %s\n", path);
        close(fd);
        return NULL;
    }

    /* 读取文件内容 */
    void* buffer = malloc(st.st_size);
    if (!buffer) {
        dprintf(2, "Failed to allocate memory\n");
        close(fd);
        return NULL;
    }

    ssize_t bytes_read = read(fd, buffer, st.st_size);
    close(fd);

    if (bytes_read != st.st_size) {
        dprintf(2, "Failed to read file\n");
        free(buffer);
        return NULL;
    }

    /* 检查魔数和版本 */
    struct plugin_interface* api = (struct plugin_interface*)buffer;
    if (api->magic != PLUGIN_MAGIC) {
        dprintf(2, "Invalid plugin magic: expected 0x%x, got 0x%x\n",
               PLUGIN_MAGIC, api->magic);
        free(buffer);
        return NULL;
    }

    if (api->version != PLUGIN_VERSION) {
        dprintf(2, "Plugin version mismatch: expected %d, got %d\n",
               PLUGIN_VERSION, api->version);
        free(buffer);
        return NULL;
    }

    return api;
}

int main(void) {
    char libpath[2048];
    const char* libname = "test6.dl";
    
    /* 获取当前工作目录 */
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        dprintf(1, "Current working directory: %s\n", cwd);
        /* 构建完整路径 */
        snprintf(libpath, sizeof(libpath), "%s/%s", cwd, libname);
    } else {
        dprintf(2, "Failed to get current directory\n");
        return 1;
    }
    
    /* 检查文件是否存在 */
    if (access(libpath, F_OK) != 0) {
        dprintf(2, "Error: %s does not exist\n", libpath);
        return 1;
    }
    
    dprintf(1, "File %s exists, attempting to load...\n", libpath);
    
    /* 加载插件 */
    struct plugin_interface* api = load_plugin(libpath);
    if (!api) {
        dprintf(2, "Failed to load plugin\n");
        return 1;
    }
    
    dprintf(1, "Successfully loaded plugin\n");
    
    /* 分配可执行内存 */
    void* exec_mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec_mem == MAP_FAILED) {
        dprintf(2, "Failed to allocate executable memory\n");
        free(api);
        return 1;
    }

    /* 复制函数代码 */
    memcpy(exec_mem, api->code, sizeof(api->code));
    
    /* 调用插件函数 */
    int (*test_func)(void) = (int (*)(void))exec_mem;
    int result = test_func();
    dprintf(1, "test_func() returned: %d\n", result);
    
    /* 清理 */
    munmap(exec_mem, 4096);
    free(api);
    dprintf(1, "Plugin unloaded\n");
    
    return 0;
} 