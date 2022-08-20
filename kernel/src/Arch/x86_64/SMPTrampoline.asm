%include "smpdefines.inc"

SMP_TRAMPOLINE_ENTRY64 equ (_smp_trampoline_entry64 - _smp_trampoline_entry16 + SMP_TRAMPOLINE_ENTRY)

global _smp_trampoline_entry16
global _smp_trampoline_end

section .data
_smp_trampoline_entry16:
BITS 16

; This will get moved to 0x2000 in physical RAM

cli
cld

mov ax, SMP_MAGIC
mov word [SMP_TRAMPOLINE_DATA_START_FLAG], ax

mov eax, cr4
or eax, 1 << 5  ; Set PAE bit
mov cr4, eax

mov eax, dword [SMP_TRAMPOLINE_CR3]
mov cr3, eax

mov ecx, 0xC0000080 ; EFER Model Specific Register
rdmsr               ; Read from the MSR 
or eax, 1 << 8
or eax, 1 ; syscall enable
wrmsr

mov eax, cr0
or eax, 0x80000001 ; Paging, Protected Mode
mov cr0, eax

lgdt [SMP_TRAMPOLINE_GDT_PTR]

jmp 0x08:(SMP_TRAMPOLINE_ENTRY64)

hlt

BITS 64
_smp_trampoline_entry64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, [SMP_TRAMPOLINE_STACK]

    mov rax, cr0
	and ax, 0xFFFB		; Clear coprocessor emulation
	or ax, 0x2			; Set coprocessor monitoring
	mov cr0, rax

	;Enable SSE
	mov rax, cr4
	or ax, 3 << 9		; Set flags for SSE
	mov cr4, rax

    xor rbp, rbp
    mov rdi, [SMP_TRAMPOLINE_CPU_ID]

    call [SMP_TRAMPOLINE_ENTRY2]

    cli
    hlt

_smp_trampoline_end:
