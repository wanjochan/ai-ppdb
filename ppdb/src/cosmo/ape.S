.section .text,"ax",@progbits
.globl _ape_start
_ape_start:
    # 简单的入口点
    xor %eax, %eax
    ret

.section .data,"aw",@progbits
    # 一些数据定义
    .align 8
    .globl some_data
some_data:
    .quad 0x1234567890abcdef

.section .bss,"aw",@nobits
    # BSS段定义
    .align 8
    .globl some_bss
some_bss:
    .zero 8 