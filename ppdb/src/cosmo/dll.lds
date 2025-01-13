/* APE DLL linker script */
ENTRY(module_main)

PHDRS {
    headers PT_LOAD FLAGS(4);  /* r-- */
    text PT_LOAD FLAGS(5);     /* r-x */
    data PT_LOAD FLAGS(6);     /* rw- */
}

SECTIONS {
    . = 0x220000000;
    
    .head : {
        __executable_start = .;
        /* DOS MZ header */
        QUAD(0x00005A4D)
        . = __executable_start + 0x3c;
        LONG(0x40)            /* e_lfanew */
        
        /* PE header */
        . = __executable_start + 0x40;
        LONG(0x4550)         /* PE magic */
        SHORT(0x8664)        /* AMD64 */
        SHORT(6)             /* sections */
        LONG(0)             /* timestamp */
        LONG(0)             /* symbol table */
        LONG(0)             /* symbols */
        SHORT(0xf0)         /* optional header size */
        SHORT(0x2022)       /* characteristics: DLL, large address aware */
        
        /* Optional header */
        SHORT(0x20b)        /* PE32+ */
        BYTE(1)             /* linker version */
        BYTE(0)
        LONG(SIZEOF(.text)) /* code size */
        LONG(SIZEOF(.data)) /* initialized data size */
        LONG(SIZEOF(.bss))  /* uninitialized data size */
        LONG(module_main - 0x220000000) /* entry point */
        LONG(SIZEOF(.text)) /* base of code */
        
        /* PE+ specific fields */
        QUAD(0x220000000)   /* image base */
        LONG(4096)          /* section alignment */
        LONG(512)           /* file alignment */
        SHORT(6)            /* OS major */
        SHORT(0)            /* OS minor */
        SHORT(0)            /* image major */
        SHORT(0)            /* image minor */
        SHORT(6)            /* subsystem major */
        SHORT(0)            /* subsystem minor */
        LONG(0)             /* Win32 version */
        LONG(SIZEOF(.head) + SIZEOF(.text) + SIZEOF(.rdata) + SIZEOF(.data) + SIZEOF(.bss)) /* image size */
        LONG(SIZEOF(.head)) /* headers size */
        LONG(0)             /* checksum */
        SHORT(3)            /* subsystem (Windows CUI) */
        SHORT(0x400)        /* DLL characteristics */
        QUAD(0x100000)      /* stack reserve */
        QUAD(0x1000)        /* stack commit */
        QUAD(0x100000)      /* heap reserve */
        QUAD(0x1000)        /* heap commit */
        LONG(0)             /* loader flags */
        LONG(16)            /* number of data directories */
        
        /* ELF header */
        . = __executable_start + 0x80;
        BYTE(0x7F) BYTE(0x45) BYTE(0x4C) BYTE(0x46)
        BYTE(2)             /* 64-bit */
        BYTE(1)             /* little endian */
        BYTE(1)             /* version */
        BYTE(0)             /* System V ABI */
        QUAD(0)             /* padding */
        SHORT(3)            /* ET_DYN */
        SHORT(0x3E)         /* x86-64 */
        LONG(1)             /* version */
        QUAD(module_main)   /* entry point */
        
        /* Mach-O header */
        . = __executable_start + 0xC0;
        LONG(0xFEEDFACF)    /* MH_MAGIC_64 */
        LONG(0x1000007)     /* CPU_TYPE_X86_64 */
        LONG(0x3)           /* CPU_SUBTYPE_X86_64_ALL */
        LONG(6)             /* MH_DYLIB */
        LONG(4)             /* number of load commands */
        LONG(0)             /* size of load commands */
        LONG(0x2000)        /* flags */
        LONG(0)             /* reserved */
        
        . = ALIGN(4096);
    } :headers
    
    .text : {
        __text_start = .;
        *(.text.startup)
        *(.text.hot)
        *(.text)
        *(.text.*)
        __text_end = .;
        . = ALIGN(4096);
    } :text
    
    .rdata : {
        __rdata_start = .;
        *(.rdata)
        *(.rodata)
        *(.rodata.*)
        __rdata_end = .;
        . = ALIGN(4096);
    } :text
    
    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.got)
        *(.got.plt)
        __data_end = .;
        . = ALIGN(4096);
    } :data
    
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
        . = ALIGN(4096);
    } :data
    
    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
        *(.debug*)
        *(.gcc_except_table*)
    }
}

PROVIDE(_GLOBAL_OFFSET_TABLE_ = .);
dll_cod_offset = 0;
dll_cod_vaddr = ADDR(.head);
dll_cod_filesz = SIZEOF(.head) + SIZEOF(.text) + SIZEOF(.rdata);
dll_cod_memsz = dll_cod_filesz;
dll_cod_align = 4096; 