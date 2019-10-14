BITS 64

extern isr_handler
extern irq_handler

global idt_flush

extern idt_ptr

%macro pushaq 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popaq 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

idt_flush:
    lidt[idt_ptr]
	ret
    ;add rsp, 8
    iretq

test__:
    ret

%macro ISR_ERROR_CODE 1
	global isr%1
	isr%1:
        cli
        call test__
        push qword [rsp+5*8]
        push qword [rsp+5*8]
        push qword [rsp+5*8]
        push qword [rsp+5*8]
        push qword [rsp+5*8]
        pushaq
        mov rdi, %1
        mov rsi, rsp
        mov rdx, qword [rsp+20*8]
        call isr_handler
        popaq
        iretq
%endmacro

%macro ISR_NO_ERROR_CODE 1
	global isr%1
	isr%1:
		cli
        call test__
        pushaq
        mov rdi, %1
        mov rsi, rsp
        xor rdx, rdx
        call isr_handler
        popaq
        iretq
%endmacro

%macro IRQ 2
  global irq%1
  irq%1:
    cli
    pushaq
    mov rdi, %2
    mov rsi, rsp
    xor rdx, rdx
    call irq_handler
    popaq
    iretq
%endmacro

ISR_NO_ERROR_CODE  0
ISR_NO_ERROR_CODE  1
ISR_NO_ERROR_CODE  2
ISR_NO_ERROR_CODE  3
ISR_NO_ERROR_CODE  4
ISR_NO_ERROR_CODE  5
ISR_NO_ERROR_CODE  6
ISR_NO_ERROR_CODE  7
ISR_ERROR_CODE 8
ISR_NO_ERROR_CODE  9
ISR_ERROR_CODE 10
ISR_ERROR_CODE 11
ISR_ERROR_CODE 12
ISR_ERROR_CODE 13
ISR_ERROR_CODE 14
ISR_NO_ERROR_CODE  15
ISR_NO_ERROR_CODE  16
ISR_ERROR_CODE  17
ISR_NO_ERROR_CODE  18
ISR_NO_ERROR_CODE 19
ISR_NO_ERROR_CODE 20
ISR_NO_ERROR_CODE 21
ISR_NO_ERROR_CODE 22
ISR_NO_ERROR_CODE 23
ISR_NO_ERROR_CODE 24
ISR_NO_ERROR_CODE 25
ISR_NO_ERROR_CODE 26
ISR_NO_ERROR_CODE 27
ISR_NO_ERROR_CODE 28
ISR_NO_ERROR_CODE 29
ISR_ERROR_CODE 30
ISR_NO_ERROR_CODE 31
ISR_NO_ERROR_CODE 32
ISR_NO_ERROR_CODE 0x69 ; Syscall

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47