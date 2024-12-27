/*
 * TinyCC Cosmopolitan Configuration
 */

#ifndef _CONFIG_COSMO_H
#define _CONFIG_COSMO_H

/* 版本信息 */
#define TCC_VERSION "0.9.27-cosmo"

/* 目标平台 */
#define TCC_TARGET_X86_64 1

/* 基本配置 */
#define CONFIG_TCC_STATIC 1  /* 静态链接 */
#define CONFIG_USE_LIBGCC 0  /* 不使用 libgcc */
#define CONFIG_TCC_BACKTRACE 1  /* 支持堆栈回溯 */

/* 优化选项 */
#define CONFIG_TCC_OPTIMIZE 2  /* 基本优化 */

/* 调试信息 */
#define CONFIG_TCC_DEBUG 1

/* 系统配置 */
#define CONFIG_COSMO 1  /* Cosmopolitan 支持 */
#define CONFIG_TCC_APE 1  /* APE 格式支持 */

/* 内存管理 */
#define TCC_PAGE_SIZE 4096
#define TCC_MALLOC_ALIGN 16

/* 路径配置 */
#define CONFIG_SYSROOT ""
#define CONFIG_TCCDIR "/lib/tcc"

/* 系统头文件路径 */
#define CONFIG_TCC_SYSINCLUDEPATHS "/usr/include:/usr/local/include"
#define CONFIG_TCC_LIBPATHS "/usr/lib:/usr/local/lib"
#define CONFIG_TCC_CRTPREFIX "/usr/lib"
#define CONFIG_TCC_ELFINTERP "/lib/ld-linux.so"

/* 编译器特性 */
#define TCC_IS_NATIVE
#define CONFIG_TCC_SEMLOCK 0
#define CONFIG_TCC_ASM 1

/* 禁用不需要的功能 */
#undef CONFIG_TCC_PIE  /* 禁用 PIE */
#undef CONFIG_TCC_PIC  /* 禁用 PIC */
#undef CONFIG_TCC_BCHECK  /* 禁用边界检查 */

#endif /* _CONFIG_COSMO_H */
