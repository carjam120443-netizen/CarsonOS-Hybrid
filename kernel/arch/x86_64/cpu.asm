BITS 64
DEFAULT REL

SECTION .text
GLOBAL arch_outb
GLOBAL arch_inb
GLOBAL arch_inw
GLOBAL arch_outw
GLOBAL arch_outl
GLOBAL arch_inl
GLOBAL arch_disable_interrupts
GLOBAL arch_halt

; void arch_outb(uint16_t port, uint8_t value)
; SysV AMD64: port in DI, value in SIL.
arch_outb:
    mov dx, di
    mov al, sil
    out dx, al
    ret

; uint8_t arch_inb(uint16_t port)
; SysV AMD64: port in DI, result in AL.
arch_inb:
    mov dx, di
    xor eax, eax
    in al, dx
    ret

; uint16_t arch_inw(uint16_t port)
arch_inw:
    mov dx, di
    xor eax, eax
    in ax, dx
    ret

arch_disable_interrupts:
    cli
    ret

arch_halt:
    hlt
    ret

; uint32_t arch_inl(uint16_t port)
arch_inl:
    mov dx, di
    in eax, dx
    ret

; void arch_outw(uint16_t port, uint16_t value)
arch_outw:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

; void arch_outl(uint16_t port, uint32_t value)
arch_outl:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret
