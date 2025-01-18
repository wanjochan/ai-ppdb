#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* 插件版本和魔数 */
#define PLUGIN_VERSION 1
#define PLUGIN_MAGIC 0x50504442 /* "PPDB" */

/* 错误码定义 */
#define ERR_SUCCESS 0
#define ERR_INVALID_PARAM -1
#define ERR_OUT_OF_MEMORY -2
#define ERR_NETWORK_ERROR -3

/* 基础数据类型 */
typedef struct {
    uint32_t size;
    uint8_t* data;
} buffer_t;

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

/* 测试Core模块 */
static int test_core_module(struct plugin_interface* api, void* exec_mem) {
    dprintf(1, "\nTesting Core Module:\n");
    
    /* 测试core_init */
    memcpy(exec_mem, api->core_init, sizeof(api->core_init));
    int (*core_init)(void) = (int (*)(void))exec_mem;
    int init_result = core_init();
    dprintf(1, "core_init() returned: %d\n", init_result);
    
    /* 测试core_alloc */
    memcpy(exec_mem, api->core_alloc, sizeof(api->core_alloc));
    size_t (*core_alloc)(size_t) = (size_t (*)(size_t))exec_mem;
    size_t alloc_size = core_alloc(100);
    dprintf(1, "core_alloc(100) returned: %zu\n", alloc_size);
    
    return ERR_SUCCESS;
}

/* 测试Net模块 */
static int test_net_module(struct plugin_interface* api, void* exec_mem) {
    dprintf(1, "\nTesting Net Module:\n");
    
    /* 测试net_connect */
    memcpy(exec_mem, api->net_connect, sizeof(api->net_connect));
    int (*net_connect)(void) = (int (*)(void))exec_mem;
    int connect_result = net_connect();
    dprintf(1, "net_connect() returned: %d\n", connect_result);
    
    /* 测试net_send */
    memcpy(exec_mem, api->net_send, sizeof(api->net_send));
    size_t (*net_send)(size_t) = (size_t (*)(size_t))exec_mem;
    size_t send_result = net_send(200);
    dprintf(1, "net_send(200) returned: %zu\n", send_result);
    
    return ERR_SUCCESS;
}

int main(void) {
    char libpath[2048];
    const char* libname = "test7.dl";
    
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

    /* 测试Core模块 */
    if (test_core_module(api, exec_mem) != ERR_SUCCESS) {
        dprintf(2, "Core module test failed\n");
        goto cleanup;
    }
    
    /* 测试Net模块 */
    if (test_net_module(api, exec_mem) != ERR_SUCCESS) {
        dprintf(2, "Net module test failed\n");
        goto cleanup;
    }
    
    dprintf(1, "\nAll tests completed successfully\n");
    
cleanup:
    /* 清理 */
    munmap(exec_mem, 4096);
    free(api);
    dprintf(1, "Plugin unloaded\n");
    
    return 0;
} 