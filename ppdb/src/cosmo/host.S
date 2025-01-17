.section .text

.globl _HOSTXNU
.globl _HOSTLINUX
.globl _HOSTOPENBSD
.globl _HOSTFREEBSD
.globl _HOSTNETBSD

.align 8
_HOSTXNU:
    .quad 0
_HOSTLINUX:
    .quad 0
_HOSTOPENBSD:
    .quad 0
_HOSTFREEBSD:
    .quad 0
_HOSTNETBSD:
    .quad 0 