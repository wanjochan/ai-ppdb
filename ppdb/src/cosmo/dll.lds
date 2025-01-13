/* DLL linker script */
ENTRY(module_main)

SECTIONS {
    . = SIZEOF_HEADERS;
    
    .text : ALIGN(4096) {
        __text_start = .;
        *(.text.startup)
        *(.text.hot)
        *(.text)
        *(.text.*)
        __text_end = .;
    }
    
    .rdata : ALIGN(4096) {
        __rdata_start = .;
        *(.rdata)
        *(.rodata)
        *(.rodata.*)
        __rdata_end = .;
    }
    
    .data : ALIGN(4096) {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    }
    
    .idata : ALIGN(4096) {
        __idata_start = .;
        *(.idata)
        *(.idata.*)
        __idata_end = .;
    }
    
    .edata : ALIGN(4096) {
        __edata_start = .;
        *(.edata)
        *(.edata.*)
        __edata_end = .;
    }
    
    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }
    
    .reloc : ALIGN(4096) {
        *(.reloc)
    }
    
    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
        *(.debug*)
        *(.gcc_except_table*)
    }
}

PROVIDE(_GLOBAL_OFFSET_TABLE_ = .); 