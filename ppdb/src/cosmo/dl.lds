/* DL 链接脚本 */
ENTRY(test4_func)
OUTPUT_FORMAT(elf64-x86-64)

PHDRS {
    text PT_LOAD FLAGS(5);    /* r-x */
    data PT_LOAD FLAGS(6);    /* rw- */
    dynamic PT_DYNAMIC FLAGS(6);  /* rw- */
}

SECTIONS {
    . = SIZEOF_HEADERS;

    /* 通用代码段 */
    .text : {
        __text_start = .;
        *(.text.startup)
        *(.text)
        *(.text.*)
        /* Windows特定段 */
        *(.CRT*)
        __text_end = .;
    } :text

    /* 只读数据段 */
    .rodata : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        /* macOS特定段 */
        *(__TEXT,__const)
        __rodata_end = .;
    } :text

    /* 动态链接信息 */
    .dynamic : {
        __dynamic_start = .;
        *(.dynamic)
        __dynamic_end = .;
    } :data :dynamic

    /* 数据段 */
    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.data.rel.ro)
        *(.data.rel.ro.*)
        *(.got)
        *(.got.plt)
        __data_end = .;
    } :data

    /* BSS段 */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    /* 动态链接相关段 */
    .gnu.hash : {
        __gnu_hash_start = .;
        *(.gnu.hash)
        __gnu_hash_end = .;
    } :data

    .hash : {
        __hash_start = .;
        *(.hash)
        __hash_end = .;
    } :data

    .dynsym : {
        __dynsym_start = .;
        *(.dynsym)
        __dynsym_end = .;
    } :data

    .dynstr : {
        __dynstr_start = .;
        *(.dynstr)
        __dynstr_end = .;
    } :data

    .rela.dyn : {
        __rela_dyn_start = .;
        *(.rela.dyn)
        __rela_dyn_end = .;
    } :data

    /* 版本信息段 */
    .gnu.version : {
        *(.gnu.version)
    } :data

    .gnu.version_d : {
        *(.gnu.version_d)
    } :data

    .gnu.version_r : {
        *(.gnu.version_r)
    } :data

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
        *(.plt)
        *(.plt.got)
    }
}

/* 提供必要的符号 */
PROVIDE_HIDDEN(_GLOBAL_OFFSET_TABLE_ = .);
PROVIDE_HIDDEN(__executable_start = SIZEOF_HEADERS);
PROVIDE_HIDDEN(__ehdr_start = 0); 