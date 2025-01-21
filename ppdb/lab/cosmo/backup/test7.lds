/* 链接脚本 */
OUTPUT_FORMAT("elf64-x86-64")

PHDRS {
    plugin PT_LOAD FLAGS(5);    /* R + X */
    text PT_LOAD FLAGS(5);      /* R + X */
    rodata PT_LOAD FLAGS(4);    /* R */
    data PT_LOAD FLAGS(6);      /* R + W */
}

SECTIONS
{
    . = SIZEOF_HEADERS;

    .plugin : ALIGN(0x1000) {
        PROVIDE(__plugin_start = .);
        *(.plugin*)
        PROVIDE(__plugin_end = .);
    } :plugin

    .text : ALIGN(0x1000) {
        *(.text.startup)
        *(.text*)
        *(.init.*)
        *(.fini.*)
    } :text

    .rodata : ALIGN(0x1000) {
        *(.rodata*)
        *(.eh_frame)
    } :rodata

    .data : ALIGN(0x1000) {
        *(.data*)
        *(.bss*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.note.gnu.property)
        *(.note*)
        *(.comment)
        *(.debug*)
        *(.gnu.*)
    }
} 