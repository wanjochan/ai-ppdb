#include "plugin.h"

/* 导出函数 */
__attribute__((used))
int _dl_main(void) {
    /* 返回一个固定值 */
    return 42;
}

/* 主函数 - 用于独立运行 */
int main(void) {
    return _dl_main();
} 