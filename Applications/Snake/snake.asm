section .text

global _start

extern pmain

_start:
    call pmain
end:
    jmp end
