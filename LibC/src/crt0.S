.section .text

.extern liballoc_init

.global _start
_start:
    mov $0, %rbp
	push %rbp
	push %rbp 
	mov %rsp, %rbp

	call liballoc_init

	# Argc and Argv
	push %rsi
	push %rdi

	call _init

	pop %rdi
	pop %rsi

	# Run main
	call main

	mov $1, %rax # SYS_EXIT
	int $0x69

	hang: hlt
	jmp hang

.size _start, . - _start
