/* 链接脚本 - 生成保留符号表的二进制文件 */
ENTRY(_start)
SECTIONS
{
    /* 代码段 */
    .text : ALIGN(4K) {
        *(.text.startup)
        *(.text)
        *(.text.*)
    }

    /* 只读数据段 */
    .rodata : ALIGN(4K) {
        *(.rodata)
        *(.rodata.*)
    }

    /* 数据段 */
    .data : ALIGN(4K) {
        *(.data)
        *(.data.*)
    }

    /* GOT段 */
    .got : ALIGN(4K) {
        *(.got)
        *(.got.plt)
    }

    /* BSS段 */
    .bss : ALIGN(4K) {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /* 符号表段 - 保留所有符号 */
    .symtab : {
        *(.symtab)
        *(.strtab)
    }

    /* 调试信息段 */
    .debug : {
        *(.debug*)
    }

    /* 丢弃不需要的段 */
    /DISCARD/ : {
        *(.note*)
        *(.comment)
        *(.eh_frame)
    }
} 