BITS 64

global signalTrampolineStart
global signalTrampolineEnd
signalTrampolineStart:
    mov rax, 105 ; Signal return syscall
    int 0x69
signalTrampolineEnd: