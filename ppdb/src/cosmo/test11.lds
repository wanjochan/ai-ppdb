/* 简单的链接脚本 */

/* 定义入口点 */
ENTRY(_start)

/* 定义段的属性 */
PHDRS
{
    Head PT_LOAD FLAGS(5);    /* r-x */
    Code PT_LOAD FLAGS(5);    /* r-x */
    Data PT_LOAD FLAGS(6);    /* rw- */
    BSS  PT_LOAD FLAGS(6);    /* rw- */
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
        . = ALIGN(16);
        _header_end = .;
    } :Head

    /* 代码段 */
    .text ALIGN(16) : {
        _text_start = .;

        /* dl_init函数 */
        . = ALIGN(16);
        _dl_init = .;
        *(.text.dl_init)
        . = ALIGN(16);

        /* dl_main函数 */
        _dl_main = .;
        *(.text.dl_main)
        . = ALIGN(16);

        /* dl_fini函数 */
        _dl_fini = .;
        *(.text.dl_fini)
        . = ALIGN(16);

        /* 其他代码 */
        *(.text*)
        . = ALIGN(16);
        _text_end = .;
    } :Code

    /* 数据段 */
    .data ALIGN(16) : {
        _data_start = .;
        *(.data)
        *(.data.*)
        . = ALIGN(16);
        _data_end = .;
    } :Data

    /* BSS段 */
    .bss ALIGN(16) (NOLOAD) : {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        . = ALIGN(16);
        _bss_end = .;
    } :BSS

    /* 丢弃不需要的段 */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.rela*)
        *(.gnu*)
    }

    /* 文件结束 */
    _end = .;
}

/* 计算偏移量 */
_dl_init_offset = _dl_init - _header_start;
_dl_main_offset = _dl_main - _header_start;
_dl_fini_offset = _dl_fini - _header_start; 