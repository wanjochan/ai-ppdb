/* 简单的链接脚本 */
ENTRY(_start)
PHDRS {
    header PT_LOAD FLAGS(4);  /* r-- */
    text   PT_LOAD FLAGS(5);  /* r-x */
    data   PT_LOAD FLAGS(6);  /* rw- */
    priv   PT_LOAD FLAGS(7);  /* rwx */
}

SECTIONS
{
    /* 从地址0开始布局 */
    . = 0x0;

    /* 头部段 */
    .header : {
        _header_start = .;
        /* Magic: PPDB */
        BYTE(0x42)  /* B */
        BYTE(0x44)  /* D */
        BYTE(0x50)  /* P */
        BYTE(0x50)  /* P */
        /* Version: 1 */
        BYTE(0x01)
        BYTE(0x00)
        BYTE(0x00)
        BYTE(0x00)
        /* 偏移量 */
        LONG(_dl_init_offset)
        LONG(_dl_main_offset)
        LONG(_dl_fini_offset)
        . = ALIGN(8);
        _header_end = .;
    } :header

    /* 代码段 */
    .text ALIGN(8) : {
        _text_start = .;

        /* 插件函数 */
        . = ALIGN(8);
        _dl_init = .;
        KEEP(*(.text.dl_init))
        . = ALIGN(8);
        _dl_main = .;
        KEEP(*(.text.dl_main))
        . = ALIGN(8);
        _dl_fini = .;
        KEEP(*(.text.dl_fini))

        /* 其他代码 */
        . = ALIGN(8);
        *(.text*)
        *(.rodata*)
        . = ALIGN(8);
        _text_end = .;
    } :text

    /* 数据段 */
    .data ALIGN(8) : {
        _data_start = .;
        *(.data*)
        . = ALIGN(8);
        _data_end = .;
    } :data

    /* BSS段 */
    .bss ALIGN(8) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } :data

    /* 初始化段 */
    .init : {
        KEEP(*(.init))
        KEEP(*(.initprologue))
        KEEP(*(.init.*))
    } :text

    /* 特权段 */
    .privileged ALIGN(8) : {
        __privileged_start = .;
        *(.privileged)
        *(.privileged.*)
        . = ALIGN(8);
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

/* 计算偏移量 */
_dl_init_offset = _dl_init - _header_start;
_dl_main_offset = _dl_main - _header_start;
_dl_fini_offset = _dl_fini - _header_start; 