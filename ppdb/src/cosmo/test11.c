/* 简单的插件示例 */

/* 插件函数 */
__attribute__((section(".text.dl_init")))
__attribute__((used))
int dl_init(void) {
    return 0;
}

__attribute__((section(".text.dl_main")))
__attribute__((used))
int dl_main(void) {
    return 42;
}

__attribute__((section(".text.dl_fini")))
__attribute__((used))
int dl_fini(void) {
    return 0;
} 