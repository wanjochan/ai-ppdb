#include "cosmopolitan.h"

// 获取平台名称
static const char* get_platform_name(void) {
    if (IsWindows()) return "Windows";
    if (IsLinux()) return "Linux";
    if (IsMacho()) return "macOS";
    return "Unknown Platform";
}

// 系统信息测试
static void print_system_info(void) {
    printf("Platform: %s\n", get_platform_name());
    printf("CPU: %s\n", IsX86_64() ? "x86_64" : "other");
    printf("Endian: %s\n", IsLittleEndian() ? "little" : "big");
}

// 字符串处理测试
static char* str_reverse(const char* str) {
    static char buffer[256];
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        buffer[i] = str[len - 1 - i];
    }
    buffer[len] = '\0';
    return buffer;
}

// 跨平台文件操作测试
static bool write_test_file(void) {
    FILE *f = fopen("test3.txt", "w");
    if (!f) return false;
    fprintf(f, "Hello from %s!\n", get_platform_name());
    fclose(f);
    return true;
}

// 主入口函数
int module_main(void) {
    // 测试平台检测
    print_system_info();
    
    // 测试字符串处理
    const char* orig = "Hello, Cosmopolitan!";
    const char* rev = str_reverse(orig);
    printf("Original: %s\n", orig);
    printf("Reversed: %s\n", rev);
    
    // 测试文件操作
    bool file_ok = write_test_file();
    printf("File write test: %s\n", file_ok ? "OK" : "Failed");
    
    // 返回测试结果
    return file_ok ? 42 : -1;
}
