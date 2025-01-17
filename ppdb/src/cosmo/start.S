.section .text

.globl ElfEntrypoint
.globl XnuEntrypoint

XnuEntrypoint:
    leaq _HOSTXNU(%rip), %rdx
    movb (%rdx), %dl
ElfEntrypoint:
    movq %rsp, %rsi
    andq $-16, %rsp
    call ApeLoader 