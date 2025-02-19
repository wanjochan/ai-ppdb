/* 主程序链接脚本 */
ENTRY(_start)
PHDRS {
    ape    PT_LOAD FLAGS(5);  /* r-x */
    text   PT_LOAD FLAGS(5);  /* r-x */
    data   PT_LOAD FLAGS(6);  /* rw- */
    priv   PT_LOAD FLAGS(7);  /* rwx */
}

SECTIONS
{
    /* 从地址0开始布局 */
    . = 0x0;

    /* APE头部 */
    .ape : {
        QUAD(0x1122334455667788)  /* magic */
        QUAD(0)                   /* entry */
        QUAD(0)                   /* text */
        QUAD(0)                   /* data */
        QUAD(0)                   /* flags */
        . = ALIGN(4096);
    } :ape

    /* 代码段 */
    .text ALIGN(4096) : {
        _text_start = .;
        *(.text*)
        *(.rodata*)
        . = ALIGN(4096);
        _text_end = .;
    } :text

    /* 数据段 */
    .data ALIGN(4096) : {
        _data_start = .;
        *(.data*)
        . = ALIGN(4096);
        _data_end = .;
    } :data

    /* BSS段 */
    .bss ALIGN(4096) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4096);
        _bss_end = .;
    } :data

    /* 初始化段 */
    .init : {
        KEEP(*(.init))
        KEEP(*(.initprologue))
        KEEP(*(.init.*))
    } :text

    /* 特权段 */
    .privileged ALIGN(4096) : {
        __privileged_start = .;
        *(.privileged)
        *(.privileged.*)
        . = ALIGN(4096);
        __privileged_end = .;
    } :priv

    /* 丢弃不需要的段 */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.rela*)
        *(.gnu*)
        *(.debug*)
        *(.pe*)
        *(.macho*)
        *(.fini*)
    }

    /* 文件结束 */
    _end = .;
} 