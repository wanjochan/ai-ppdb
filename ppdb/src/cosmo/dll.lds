/* DLL linker script */
ENTRY(module_main)

SECTIONS {
    . = SIZEOF_HEADERS;
    
    .text : {
        *(.text*)
    }
    
    .rodata : {
        *(.rodata*)
    }
    
    .data : {
        *(.data*)
    }
    
    .bss : {
        *(.bss*)
        *(COMMON)
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