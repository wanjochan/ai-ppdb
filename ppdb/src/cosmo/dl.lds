/* DL 链接脚本 */
ENTRY(test4_func)
OUTPUT_FORMAT(elf64-x86-64)

PHDRS {
    text PT_LOAD FLAGS(5);    /* r-x */
    data PT_LOAD FLAGS(6);    /* rw- */
}

SECTIONS {
    . = 0x220000000;

    .text : ALIGN(4096) {
        __text_start = .;
        *(.text.startup)
        *(.text)
        *(.text.*)
        __text_end = .;
    } :text

    .rodata : ALIGN(4096) {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        __rodata_end = .;
    } :text

    .data : ALIGN(4096) {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    } :data

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    .symtab : {
        *(.symtab)
        *(.strtab)
        *(.shstrtab)
    }

    /DISCARD/ : {
        *(.note.*)
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.debug_*)
        *(.gcc_except_table)
        *(.init)
        *(.fini)
        *(.rela.*)
        *(.got)
        *(.got.plt)
        *(.plt)
        *(.plt.got)
    }
}

/* 提供必要的符号 */
PROVIDE_HIDDEN(_GLOBAL_OFFSET_TABLE_ = .);
PROVIDE_HIDDEN(__executable_start = 0x220000000);
PROVIDE_HIDDEN(__ehdr_start = __executable_start);
PROVIDE_HIDDEN(ape_stack_vaddr = 0x700000000000);
PROVIDE_HIDDEN(ape_stack_memsz = 0x100000);
PROVIDE_HIDDEN(ape_stack_round = 0x1000);

/* PE 相关符号 */
PROVIDE_HIDDEN(ape_pe_shnum = 0);
PROVIDE_HIDDEN(ape_pe_optsz = 0);
PROVIDE_HIDDEN(ape_pe_entry = __text_start);
PROVIDE_HIDDEN(ape_pe_base = 0x140000000);
PROVIDE_HIDDEN(ape_pe_sectionalignment = 4096);
PROVIDE_HIDDEN(ape_pe_filealignment = 512);
PROVIDE_HIDDEN(ape_pe_sizeofheaders = SIZEOF(.text));
PROVIDE_HIDDEN(ape_pe_sizeofimage = ALIGN(SIZEOF(.text) + SIZEOF(.data), 4096));

/* 其他必要符号 */
PROVIDE_HIDDEN(v_ntversion = 0x00000004);
PROVIDE_HIDDEN(v_ntsubversion = 0x00000000);
PROVIDE_HIDDEN(v_ntsubsystem = 0x0003);
PROVIDE_HIDDEN(v_ntdllchar = 0x0102);
PROVIDE_HIDDEN(ape_idata = 0);
PROVIDE_HIDDEN(ape_idata_idtsize = 0);
PROVIDE_HIDDEN(ape_text_memsz = SIZEOF(.text));
PROVIDE_HIDDEN(ape_text_rva = __text_start);
PROVIDE_HIDDEN(ape_text_filesz = SIZEOF(.text));
PROVIDE_HIDDEN(ape_text_offset = 0);
PROVIDE_HIDDEN(ape_rom_rva = __rodata_start);
PROVIDE_HIDDEN(ape_ram_rva = __data_start);
PROVIDE_HIDDEN(v_ape_realdwords = 0);
PROVIDE_HIDDEN(v_ape_allbytes = 0);
PROVIDE_HIDDEN(_edata = __data_end);

/* 初始化相关符号 */
PROVIDE_HIDDEN(__init_rodata_start = __rodata_start);
PROVIDE_HIDDEN(__init_bss_start = __bss_start); 