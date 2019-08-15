.section .text

.extern liballoc_init

.global _start
_start:
    mov $0, %ebp
	push %ebp
	push %ebp 
	mov %esp, %ebp

	call liballoc_init

	# Argc and Argv
	push %esi
	push %edi

	call _init

	pop %edi
	pop %esi

	# Run main
	call main

	mov $0, %eax # SYS_EXIT
	int $0x69

	hang: hlt
	jmp hang

.size _start, . - _start