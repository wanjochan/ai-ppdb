/* 链接脚本 - 生成保留符号表的二进制文件 */
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

/* APE相关定义 */
ape_stack_memsz = 0x100000;
ape_stack_round = 0x1000;
ape_stack_vaddr = 0x700000000000;

PHDRS {
    text PT_LOAD FLAGS(5);      /* R-X */
    rodata PT_LOAD FLAGS(4);    /* R-- */
    data PT_LOAD FLAGS(6);      /* RW- */
}

SECTIONS {
    . = 0x400000;

    /* 代码段 */
    .text : ALIGN(4K) {
        KEEP(*(.text.startup))
        *(.text)
        *(.text.*)
        *(.init)
        *(.fini)
    } :text

    /* 只读数据段 */
    .rodata : ALIGN(4K) {
        *(.rodata)
        *(.rodata.*)
        *(.rodata.str*)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.gcc_except_table)
        *(.tdata)
        *(.tbss)
        *(.preinit_array)
        *(.init_array)
        *(.fini_array)
        . = ALIGN(4K);
    } :rodata

    /* 数据段 */
    .data : ALIGN(4K) {
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.got.plt)
        *(.got)
    } :data

    /* BSS段 */
    .bss : ALIGN(4K) {
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(COMMON)
        . = ALIGN(4K);
    } :data

    /* 符号表段 - 保留所有符号 */
    .symtab : {
        *(.symtab)
    }
    .strtab : {
        *(.strtab)
    }
    .shstrtab : {
        *(.shstrtab)
    }

    /* 丢弃不需要的段 */
    /DISCARD/ : {
        *(.note*)
        *(.comment)
        *(.plt)
        *(.interp)
        *(.gnu*)
        *(.hash)
        *(.dynsym)
        *(.dynstr)
        *(.dynamic)
        *(.rela.*)
    }
} 