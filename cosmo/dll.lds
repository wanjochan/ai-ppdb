/* DLL linker script */
ENTRY(module_main)

MEMORY {
    ram (rwx) : ORIGIN = 0x220000000, LENGTH = 0x1000000
}

PHDRS {
    text PT_LOAD FLAGS(5);   /* R-X */
    data PT_LOAD FLAGS(6);   /* RW- */
}

SECTIONS {
    . = 0x220000000;

    .text : {
        *(.text*)
    } >ram :text

    .rodata : {
        *(.rodata*)
    } >ram :text

    .data : {
        *(.data*)
    } >ram :data

    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    } >ram :data

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
        *(.debug*)
        *(.gcc_except_table*)
    }
}

PROVIDE(_GLOBAL_OFFSET_TABLE_ = .); 