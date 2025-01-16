/* ELF插件链接脚本 */
ENTRY(_dl_main)
PHDRS
{
    text PT_LOAD FLAGS(7);    /* 代码段: 可读可写可执行 */
    dynamic PT_DYNAMIC;       /* 动态链接信息 */
}

SECTIONS
{
    . = 0x1000;
    
    /* 代码段 */
    .text : {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        *(.data)
        *(.data.*)
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :text

    /* 动态链接信息 */
    .dynamic : {
        *(.dynamic)
        *(.dynsym)
        *(.dynstr)
        *(.rela.*)
        *(.hash)
        *(.gnu.hash)
    } :text :dynamic

    /* 丢弃调试信息 */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.debug*)
    }
} 