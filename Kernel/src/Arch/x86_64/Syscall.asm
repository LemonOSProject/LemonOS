global syscall_entry

extern SyscallHandler

section .text

USER_SS equ 0x1B
USER_CS equ 0x23

CPU_LOCAL_SELF equ 0x0
CPU_LOCAL_ID equ 0x8
CPU_LOCAL_THREAD equ 0x10
CPU_LOCAL_SCRATCH equ 0x18
CPU_LOCAL_TSS equ 0x20
CPU_LOCAL_TSS_RSP0 equ (CPU_LOCAL_TSS + 0x4)

syscall_entry:
    swapgs
    mov qword [gs:CPU_LOCAL_SCRATCH], rsp
    mov rsp, qword [gs:CPU_LOCAL_TSS_RSP0]
    push USER_SS
    push qword [gs:CPU_LOCAL_SCRATCH]
    swapgs
    push r11
    push USER_CS
    push rcx
    push 0

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

    mov rdi, rsp
    xor rbp, rbp
    call SyscallHandler

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

    add rsp, 8
    ; Get user RIP
    pop rcx
    add rsp, 8
    ; User RFLAGS
    pop r11
    ; Get user RSP
    pop rsp
    o64 sysret
