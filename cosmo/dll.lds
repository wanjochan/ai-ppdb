/* DLL linker script */
ENTRY(module_main)
PHDRS {
  text PT_LOAD FLAGS(7);
  data PT_LOAD FLAGS(6);
}
SECTIONS {
  . = 0x10000000;

  /* Code sections */
  .text : {
    *(.text.module_main)
    *(.text.test_func1)
    *(.text.test_func2)
    *(.text*)
    *(.plt*)
    *(.iplt*)
  } :text

  /* Read-only data */
  .rodata : {
    *(.rodata*)
    *(.rela*)
  } :text

  /* Read-write data */
  .data : {
    *(.data*)
    *(.got*)
    *(.igot*)
  } :data

  /* BSS section */
  .bss : {
    *(.bss*)
    *(COMMON)
  } :data

  /* Discarded sections */
  /DISCARD/ : {
    *(.note.*)
    *(.comment*)
    *(.eh_frame*)
    *(.debug*)
  }
} 