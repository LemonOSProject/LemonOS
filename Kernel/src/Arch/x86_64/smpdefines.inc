;// Use some trickery here
;/* GCC will ignore the following code and NASM will ignore the comment start
%ifdef beeeeeeeeeeeeeef
;*/
#define smpdef(x, y) \
    const unsigned x = y;
;/*
%else
%define smpdef(x, y) x equ (y)
%endif
;*/

smpdef(SMP_TRAMPOLINE_ENTRY, 0x2000);
smpdef(SMP_TRAMPOLINE_DATA_ADDR, 0x1000);
smpdef(SMP_TRAMPOLINE_DATA_START_FLAG, SMP_TRAMPOLINE_DATA_ADDR);
smpdef(SMP_TRAMPOLINE_CPU_ID, (SMP_TRAMPOLINE_DATA_ADDR + 0x2));
smpdef(SMP_TRAMPOLINE_GDT_PTR, (SMP_TRAMPOLINE_DATA_ADDR + 0x10));
smpdef(SMP_TRAMPOLINE_CR3, (SMP_TRAMPOLINE_DATA_ADDR + 0x20));
smpdef(SMP_TRAMPOLINE_STACK, (SMP_TRAMPOLINE_DATA_ADDR + 0x28));
smpdef(SMP_TRAMPOLINE_ENTRY2, (SMP_TRAMPOLINE_DATA_ADDR + 0x30));
smpdef(SMP_MAGIC, 0xB33F);
