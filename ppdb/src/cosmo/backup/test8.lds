/* 链接脚本 */
OUTPUT_FORMAT("elf64-x86-64")

PHDRS {
    text PT_LOAD FLAGS(5);    /* R-X */
    rodata PT_LOAD FLAGS(4);  /* R-- */
    data PT_LOAD FLAGS(6);    /* RW- */
    dynamic PT_DYNAMIC FLAGS(6);  /* RW- */
}

SECTIONS
{
    . = ALIGN(4096);

    .text : {
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :rodata

    .data : {
        *(.data*)
    } :data

    .bss : {
        *(.bss*)
        *(COMMON)
    } :data

    .gnu.hash : {
        *(.gnu.hash)
    } :rodata

    .dynsym : {
        *(.dynsym)
    } :rodata

    .dynstr : {
        *(.dynstr)
    } :rodata

    .dynamic : {
        *(.dynamic)
    } :dynamic :data

    .got : {
        *(.got*)
    } :data

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
        *(.debug*)
    }
} 