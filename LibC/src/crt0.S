.section .text

.global _start
_start:
    movq $0, %rbp
	pushq %rbp
	pushq %rbp 
	movq %rsp, %rbp

	call _init

	# Run main
	call main

	#call _fini

	#mov $1, %rax # SYS_EXIT
	#int $0x69

	hang: hlt
	jmp hang

.size _start, . - _start
