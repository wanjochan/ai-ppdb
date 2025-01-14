/* 链接脚本 */
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

    .text : {
        *(.text.startup)
        *(.text.hot)
        *(.text)
        *(.text.*)
    } :text

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
    } :rodata

    . = ALIGN(4096);
    .data : {
        *(.data)
        *(.data.*)
        *(.data.rel.ro)
        *(.data.rel.ro.*)
        *(.got)
        *(.got.plt)
    } :data

    . = ALIGN(4096);
    .dynamic : {
        *(.dynamic)
    } :dynamic :data

    . = ALIGN(4096);
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.debug*)
        *(.gcc_except_table*)
    }
} 