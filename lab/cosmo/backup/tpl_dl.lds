OUTPUT_FORMAT("elf64-x86-64")

PHDRS {
  text PT_LOAD FLAGS(5);    /* 读+执行 */
  data PT_LOAD FLAGS(6);    /* 读+写 */
}

SECTIONS
{
  . = SIZEOF_HEADERS;
  
  .text : {
    *(.text)
    *(.text.*)
    . = ALIGN(16);
  } :text
  
  .rodata : {
    *(.rodata)
    *(.rodata.*)
    . = ALIGN(16);
  } :text
  
  .data : {
    *(.data)
    *(.data.*)
    . = ALIGN(16);
  } :data
  
  .bss : {
    *(.bss)
    *(.bss.*)
    *(COMMON)
    . = ALIGN(16);
  } :data
  
  .symtab : {
    *(.symtab)
  }
  
  .strtab : {
    *(.strtab)
  }
  
  .shstrtab : {
    *(.shstrtab)
  }
  
  /DISCARD/ : {
    *(.note.*)
    *(.comment)
    *(.eh_frame)
    *(.eh_frame_hdr)
  }
} 