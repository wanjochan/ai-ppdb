/* 链接脚本 */
OUTPUT_FORMAT("elf64-x86-64")

SECTIONS
{
    . = 0x1000;

    .plugin : {
        PROVIDE(__plugin_start = .);
        *(.plugin*)
        PROVIDE(__plugin_end = .);
    }

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
        *(.note.gnu.property)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.comment)
        *(.note*)
        *(.debug*)
        *(.gnu.*)
    }
} 