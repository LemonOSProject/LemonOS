global acquireLock
global releaseLock

acquireLock:
    lock bts dword [rdi], 0 ; Acquire lock
    jc .spin
    ret

.spin: ; If lock not available, spin
    pause
    test dword [rdi], 1
    jnz .spin
    jmp acquireLock

releaseLock:
    mov dword [rdi], 0
    ret