BITS    32

global entry
global kernel_pml4
global kernel_pdpt
global kernel_pdpt2
extern kmain

MBALIGN     equ 1<<0
MEMINFO     equ 1<<1
VIDINFO		equ 1<<2
MAGIC       equ 0x1BADB002
FLAGS       equ MBALIGN | MEMINFO | VIDINFO
CHECKSUM    equ -(MAGIC + FLAGS)

KERNEL_VIRTUAL_BASE equ 0xFFFFFFFF80000000
KERNEL_BASE_PML4_INDEX equ (((KERNEL_VIRTUAL_BASE) >> 39) & 0x1FF)
KERNEL_BASE_PDPT_INDEX equ  (((KERNEL_VIRTUAL_BASE) >> 30) & 0x1FF)

section .boot.data
align 4096
kernel_pml4:
times 512 dq 0

align 4096
kernel_pdpt:
times 512 dq 0

align 4096
kernel_pdpt2:
times 512 dq 0

align 16
GDT64:                           ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64         ; The null descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 0                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64         ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011010b                 ; Access (exec/read).
    db 00100000b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .Pointer:                    ; The GDT-pointer.
    dw $ - GDT64 - 1             ; Limit.
    dq GDT64                     ; Base.
    .Pointer64:                    ; The GDT-pointer.
    dw .Pointer - GDT64 - 1             ; Limit.
    dq GDT64 + KERNEL_VIRTUAL_BASE; Base.

section .boot.text
align 4 ; Multiboot Header
multiboot_header:
dd MAGIC
dd FLAGS
dd CHECKSUM
dd 0
dd 0
dd 0
dd 0
dd 0

dd 0
dd 0
dd 0
dd 32

no64:
  mov ecx, dword [fb_pitch]
  mov eax, ecx
  mov edx, dword [fb_height]
  mul edx
  mov ecx, eax
.fill:
  mov al, 0xFF
  mov ebx, dword [fb_addr]
  mov byte[ebx + ecx], al
  sub ecx, 3
  loop .fill
  jmp $

entry:
  mov dword [mb_addr], ebx

  mov eax, [ebx+88] ; Framebuffer pointer address in multiboot header
  mov dword [fb_addr], eax 

  mov eax, [ebx+96] ; Framebuffer pitch
  mov dword [fb_pitch], eax

  mov eax, [ebx+104] ; Framebuffer height
  mov dword [fb_height], eax

  mov eax, 0x80000000
  cpuid                  ; CPU identification.
  cmp eax, 0x80000001
  jb no64

  mov eax, 0x80000001
  cpuid                  ; CPU identification.
  test edx, 1 << 29      ; Long Mode bit is 29
  jz no64
  
  mov eax, cr4
  or eax, 1 << 5  ; Set PAE bit
  mov cr4, eax

  mov eax, kernel_pdpt ; Get address of PDPT
  or eax, 3 ; Present, Write
  mov dword [kernel_pml4], eax

  mov eax, kernel_pdpt2 ; Second PDPT
  or eax, 3
  mov dword [kernel_pml4 + KERNEL_BASE_PML4_INDEX * 8], eax

  mov eax, 0x83 ; 1 GB Pages
  mov dword [kernel_pdpt], eax

  mov eax, 0x83 ; 1 GB Pages
  mov dword [kernel_pdpt2 + KERNEL_BASE_PDPT_INDEX * 8], eax

  mov eax, kernel_pml4
  mov cr3, eax

  mov ecx, 0xC0000080 ; EFER Model Specific Register
  rdmsr               ; Read from the MSR 
  or eax, 1 << 8
  wrmsr

  mov eax, cr0
  or eax, 1 << 31
  mov cr0, eax

  lgdt [GDT64.Pointer]
  jmp GDT64.Code:entry64 - KERNEL_VIRTUAL_BASE
BITS 64

  cli
  hlt


fb_addr:
dd 0
fb_pitch:
dd 0
fb_height:
dd 0
mb_addr:
dd 0
dd 0

BITS 64
section .text
entry64:
  mov rsp, stack_top
  push rax

  lgdt [GDT64.Pointer64]

  mov ax, 0
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  push rax
  mov rax, cr0
	and ax, 0xFFFB		; Clear coprocessor emulation
	or ax, 0x2			; Set coprocessor monitoring
	mov cr0, rax

	;Enable SSE
	mov rax, cr4
	or ax, 3 << 9		; Set flags for SSE
	mov cr4, rax

  mov rdi, qword[mb_addr] ; Pass multiboot info struct
  add rdi, KERNEL_VIRTUAL_BASE
  call kmain

  cli
  hlt

section .initrd_test
global test_start
test_start: dd 0

section .bss
align 64
stack_bottom:
resb 16384
stack_top:
