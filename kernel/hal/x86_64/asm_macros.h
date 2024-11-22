#pragma once

#define PUSH_INTERRUPT_FRAME "pushq %rax \n\
    pushq %rbx \n\
    pushq %rcx \n\
    pushq %rdx \n\
    pushq %rbp \n\
    pushq %rsi \n\
    pushq %rdi \n\
    pushq %r8 \n\
    pushq %r9 \n\
    pushq %r10 \n\
    pushq %r11 \n\
    pushq %r12 \n\
    pushq %r13 \n\
    pushq %r14 \n\
    pushq %r15 \n"

// Get rid of the error code at the end
#define POP_INTERRUPT_FRAME "popq %r15 \n\
    popq %r14 \n\
    popq %r13 \n\
    popq %r12 \n\
    popq %r11 \n\
    popq %r10 \n\
    popq %r9 \n\
    popq %r8 \n\
    popq %rdi \n\
    popq %rsi \n\
    popq %rbp \n\
    popq %rdx \n\
    popq %rcx \n\
    popq %rbx \n\
    popq %rax \n\
    addq $8, %rsp \n"