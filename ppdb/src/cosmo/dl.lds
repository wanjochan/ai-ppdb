/* 链接脚本 */
OUTPUT_FORMAT("elf64-x86-64")

SECTIONS
{
    /* 代码段 */
    .text : ALIGN(16) {
        *(.text)
        *(.text.*)
    }

    /* 只读数据段 */
    .rodata : ALIGN(16) {
        *(.rodata)
        *(.rodata.*)
    }

    /* 全局偏移表 */
    .got : ALIGN(16) {
        *(.got)
        *(.got.plt)
    }

    /* 数据段 */
    .data : ALIGN(16) {
        *(.data)
        *(.data.*)
    }

    /* BSS段 */
    .bss : ALIGN(16) {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /* 符号表 */
    .symtab 0 : { *(.symtab) }
    .strtab 0 : { *(.strtab) }
    .shstrtab 0 : { *(.shstrtab) }

    /* 删除不需要的节 */
    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.gnu.*)
        *(.interp)
        *(.dynamic)
        *(.dynsym)
        *(.dynstr)
        *(.hash)
        *(.rela.*)
    }
} 