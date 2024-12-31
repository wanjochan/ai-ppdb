/* DLL linker script */
ENTRY(module_main)
PHDRS {
  text PT_LOAD FLAGS(5);   /* R-X */
  data PT_LOAD FLAGS(6);   /* RW- */
}
SECTIONS {
  . = 0x110000000;
  .text : ALIGN(16) {
    *(.text*)
  } :text
  .rodata : ALIGN(16) {
    *(.rodata*)
  } :text
  .data : ALIGN(16) {
    *(.data*)
  } :data
  .bss : ALIGN(16) {
    *(.bss*)
  } :data
  /DISCARD/ : {
    *(.note.gnu.property)
    *(.eh_frame)
    *(.comment)
  }
} 