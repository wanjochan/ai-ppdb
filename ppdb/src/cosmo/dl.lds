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

    .text : {
        __text_start = .;
        *(.text.startup)
        *(.text)
        *(.text.*)
        __text_end = .;
    } :text

    .rodata : {
        __rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        __rodata_end = .;
    } :text

    .dynamic : {
        __dynamic_start = .;
        *(.dynamic)
        __dynamic_end = .;
    } :data :dynamic

    .data.rel.ro : {
        __data_rel_ro_start = .;
        *(.data.rel.ro)
        *(.data.rel.ro.*)
        __data_rel_ro_end = .;
    } :data

    .got : {
        __got_start = .;
        *(.got)
        *(.got.plt)
        __got_end = .;
    } :data

    .data : {
        __data_start = .;
        *(.data)
        *(.data.*)
        __data_end = .;
    } :data

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

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

/* PE 相关符号 */
PROVIDE_HIDDEN(ape_pe_shnum = 0);
PROVIDE_HIDDEN(ape_pe_optsz = 0);
PROVIDE_HIDDEN(ape_pe_entry = __text_start);
PROVIDE_HIDDEN(ape_pe_base = 0);
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