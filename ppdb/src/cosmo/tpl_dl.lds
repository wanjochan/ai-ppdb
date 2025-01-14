/* 标准动态库链接脚本 */
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);    /* R-X */
    rodata PT_LOAD FLAGS(4);  /* R-- */
    data PT_LOAD FLAGS(6);    /* RW- */
    dynamic PT_DYNAMIC FLAGS(6);  /* RW- */
}

SECTIONS
{
    . = SIZEOF_HEADERS;
    . = ALIGN(4096);

    /* 代码段 */
    .text : {
        *(.text.startup)
        *(.text.hot)
        *(.text)
        *(.text.*)
    } :text

    /* 只读数据段 */
    . = ALIGN(4096);
    .rodata : {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.hash)
        *(.dynsym)
        *(.dynstr)
        *(.rela.dyn)
        *(.rela.plt)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.gcc_except_table)
    } :rodata

    /* 数据段 */
    . = ALIGN(4096);
    .data : {
        *(.data)
        *(.data.*)
        *(.data.rel.ro)
        *(.data.rel.ro.*)
        *(.got)
        *(.got.plt)
    } :data

    /* 动态段 */
    . = ALIGN(4096);
    .dynamic : {
        *(.dynamic)
    } :dynamic :data

    /* BSS段 */
    . = ALIGN(4096);
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data

    /* 丢弃不需要的段 */
    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.debug*)
    }
} 