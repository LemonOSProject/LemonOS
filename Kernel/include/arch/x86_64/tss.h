#pragma once

#include <stdint.h>

typedef struct {
    uint32_t reserved __attribute__((aligned(16)));
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved3;
    uint32_t reserved4;
    uint32_t iopbOffset;
} __attribute__((packed)) tss_t;

namespace TSS
{
    void InitializeTSS(tss_t* tss, void* gdt);

    inline void SetKernelStack(tss_t* tss, uint64_t stack){
        tss->rsp0 = stack;
    }
}