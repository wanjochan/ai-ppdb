// 导出函数声明
__attribute__((visibility("default")))
int test4_add(int a, int b) {
    return a + b;
}

static const char version_str[] = "test4 v1.0.0";

__attribute__((visibility("default")))
const char* test4_version(void) {
    return version_str;
}

// 动态库入口点
__attribute__((visibility("default")))
int module_main(void) {
    return 0;
} 