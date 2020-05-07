#pragma once

/*extern "C"{
    void acquireLock(volatile void* lock);
    void releaseLock(volatile void* lock);
}*/

#define CHECK_DEADLOCK
#ifdef CHECK_DEADLOCK
#define acquireLock(lock) ({ asm volatile(" xor %%rax, %%rax; start%=:;\
    lock btsl $0, %0; \
    jnc exit%=; \
    spin%=:; \
    pause; \
    inc %%rax; \
    cmpq $0xFFFFFFF, %%rax; \
    jg int%=; \
    testl $1, %0; \
    jnz spin%=; \
    jmp start%=; \
    int%=:; int $0x0; \
    exit%=:; " : "+m"(*lock) : "a"(0): "memory"); })
#else
#define acquireLock(lock) ({ asm volatile(" start%=:;\
    lock btsl $0, %0; \
    jnc exit%=; \
    spin%=:; \
    pause; \
    testl $1, %0; \
    jnz spin%=; \
    jmp start%=; \
    exit%=:; " : "+m"(*lock) :: "memory"); })
#endif

#define releaseLock(lock) (*lock) = 0;