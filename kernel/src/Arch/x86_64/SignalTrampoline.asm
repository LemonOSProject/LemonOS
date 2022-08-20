BITS 64

global signalTrampolineStart
global signalTrampolineEnd
signalTrampolineStart:
    mov rbp, rsp ; Set up stack frame
    mov rax, qword [rbp]
    call rax
    mov rsp, rbp
    mov rax, 105 ; Signal return syscall
    int 0x69 ; Execute syscall
signalTrampolineEnd:
