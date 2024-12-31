/* DLL linker script */
ENTRY(module_main)
PHDRS {
  text PT_LOAD FLAGS(5);   /* Read + Execute */
  data PT_LOAD FLAGS(6);   /* Read + Write */
}
SECTIONS {
  . = 0x110000000;
  
  .text : {
    *(.text.module_main)
    *(.text.test_func*)
    *(.text*)
  } :text

  .rodata : {
    *(.rodata*)
  } :text

  .data : {
    *(.data*)
  } :data

  .bss : {
    *(.bss*)
  } :data

  /DISCARD/ : {
    *(.note*)
    *(.gnu*)
    *(.comment*)
    *(.eh_frame*)
    *(.debug*)
  }
} 