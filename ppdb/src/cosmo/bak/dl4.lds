/* DL 链接脚本 */
ENTRY(test4_func)
OUTPUT_FORMAT(elf64-x86-64)

/* 设置基地址 - 降低到32位可寻址范围内 */
_BASE_ADDRESS = 0x10000000;
. = _BASE_ADDRESS;

PHDRS {
    text PT_LOAD FLAGS(5);    /* r-x */
    data PT_LOAD FLAGS(6);    /* rw- */
    note PT_NOTE FLAGS(4);    /* r-- */
}

SECTIONS {
    . = SIZEOF_HEADERS;
    . = ALIGN(4096);

    /* 代码段 */
    .text ALIGN(4096) : {
        __text_start = .;
        *(.text.startup)
        *(.text)
        *(.text.*)
        . = ALIGN(4096);
        __text_end = .;
    } :text

    /* 只读数据段 */
    .rodata ALIGN(4096) : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4096);
        __rodata_end = .;
    } :text

    /* 数据段 */
    .data ALIGN(4096) : {
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.data.rel.ro)
        *(.data.rel.ro.local)
        *(.got)
        *(.got.plt)
        *(.piro.bss.init.*)
        . = ALIGN(4096);
        __data_end = .;
    } :data

    /* BSS段 */
    .bss ALIGN(4096) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4096);
        __bss_end = .;
    } :data

    /* 重定位段 */
    .rela ALIGN(4096) : {
        *(.rela)
        *(.rela.*)
        . = ALIGN(4096);
    } :data

    /* 符号表和字符串表 */
    .symtab ALIGN(4096) : {
        __symtab_start = .;
        *(.symtab)
        __symtab_end = .;
    } :note

    .strtab ALIGN(4096) : {
        __strtab_start = .;
        *(.strtab)
        __strtab_end = .;
    } :note

    /* 节头字符串表 */
    .shstrtab ALIGN(4096) : {
        __shstrtab_start = .;
        *(.shstrtab)
        __shstrtab_end = .;
    } :note

    /* 丢弃不需要的段 */
    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.debug_*)
        *(.gcc_except_table)
        *(.init)
        *(.fini)
        *(.dynamic)
        *(.dynsym)
        *(.dynstr)
        *(.hash)
        *(.gnu.hash)
        *(.plt)
        *(.plt.got)
        *(.gnu.version*)
        *(.interp)
    }
}

/* 提供必要的符号 */
PROVIDE_HIDDEN(__executable_start = _BASE_ADDRESS + SIZEOF_HEADERS);
PROVIDE_HIDDEN(__ehdr_start = _BASE_ADDRESS); 
