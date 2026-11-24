BITS 64
DEFAULT REL

SECTION .text.entry
GLOBAL _start
EXTERN kernel_main

_start:
    ; Limine enters the kernel in 64-bit mode with a valid stack.
    ; Align it for the System V AMD64 calling convention before calling C.
    cli
    and rsp, -16
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
