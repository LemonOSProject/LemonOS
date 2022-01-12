#include <APIC.h>
#include <CString.h>
#include <HAL.h>
#include <IDT.h>
#include <IOPorts.h>
#include <Lock.h>
#include <Logging.h>
#include <Panic.h>
#include <Scheduler.h>
#include <StackTrace.h>
#include <Syscalls.h>

idt_entry_t idt[256];

idt_ptr_t idtPtr;

struct ISRDataPair {
    isr_t handler;
    void* data;
};
ISRDataPair interrupt_handlers[256];

extern "C" {
void isr0();
void isr1();
void isr2();
void isr3();
void isr4();
void isr5();
void isr6();
void isr7();
void isr8();
void isr9();
void isr10();
void isr11();
void isr12();
void isr13();
void isr14();
void isr15();
void isr16();
void isr17();
void isr18();
void isr19();
void isr20();
void isr21();
void isr22();
void isr23();
void isr24();
void isr25();
void isr26();
void isr27();
void isr28();
void isr29();
void isr30();
void isr31();
void irq0();
void irq1();
void irq2();
void irq3();
void irq4();
void irq5();
void irq6();
void irq7();
void irq8();
void irq9();
void irq10();
void irq11();
void irq12();
void irq13();
void irq14();
void irq15();
void isr0x69();
}

extern uint64_t int_vectors[];

namespace IDT {
void IPIHalt(void*, RegisterContext* r) {
    asm("cli");
    asm("hlt");
}

void InvalidInterruptHandler(void*, RegisterContext* r) { Log::Warning("Invalid interrupt handler called!"); }

static void SetGate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist = 0) {
    idt[num].base_high = (base >> 32);
    idt[num].base_med = (base >> 16) & 0xFFFF;
    idt[num].base_low = base & 0xFFFF;

    idt[num].sel = sel;
    idt[num].null = 0;
    idt[num].ist = ist & 0x7; // Interrupt Stack Table (IST)

    idt[num].flags = flags;
}

void Initialize() {
    idtPtr.limit = sizeof(idt_entry_t) * 256 - 1;
    idtPtr.base = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) {
        SetGate(i, 0, 0x08, 0x8E);
    }

    for (unsigned i = 48; i < 256; i++) {
        SetGate(i, int_vectors[i - 48], 0x08, 0x8E);
    }

    SetGate(0, (uint64_t)isr0, 0x08, 0x8E);
    SetGate(1, (uint64_t)isr1, 0x08, 0x8E);
    SetGate(2, (uint64_t)isr2, 0x08, 0x8E);
    SetGate(3, (uint64_t)isr3, 0x08, 0x8E);
    SetGate(4, (uint64_t)isr4, 0x08, 0x8E);
    SetGate(5, (uint64_t)isr5, 0x08, 0x8E);
    SetGate(6, (uint64_t)isr6, 0x08, 0x8E);
    SetGate(7, (uint64_t)isr7, 0x08, 0x8E);
    SetGate(8, (uint64_t)isr8, 0x08, 0x8E, 2); // Double Fault
    SetGate(9, (uint64_t)isr9, 0x08, 0x8E);
    SetGate(10, (uint64_t)isr10, 0x08, 0x8E);
    SetGate(11, (uint64_t)isr11, 0x08, 0x8E);
    SetGate(12, (uint64_t)isr12, 0x08, 0x8E);
    SetGate(13, (uint64_t)isr13, 0x08, 0x8E);
    SetGate(14, (uint64_t)isr14, 0x08, 0x8E);
    SetGate(15, (uint64_t)isr15, 0x08, 0x8E);
    SetGate(16, (uint64_t)isr16, 0x08, 0x8E);
    SetGate(17, (uint64_t)isr17, 0x08, 0x8E);
    SetGate(18, (uint64_t)isr18, 0x08, 0x8E);
    SetGate(19, (uint64_t)isr19, 0x08, 0x8E);
    SetGate(20, (uint64_t)isr20, 0x08, 0x8E);
    SetGate(21, (uint64_t)isr21, 0x08, 0x8E);
    SetGate(22, (uint64_t)isr22, 0x08, 0x8E);
    SetGate(23, (uint64_t)isr23, 0x08, 0x8E);
    SetGate(24, (uint64_t)isr24, 0x08, 0x8E);
    SetGate(25, (uint64_t)isr25, 0x08, 0x8E);
    SetGate(26, (uint64_t)isr26, 0x08, 0x8E);
    SetGate(27, (uint64_t)isr27, 0x08, 0x8E);
    SetGate(28, (uint64_t)isr28, 0x08, 0x8E);
    SetGate(29, (uint64_t)isr29, 0x08, 0x8E);
    SetGate(30, (uint64_t)isr30, 0x08, 0x8E);
    SetGate(31, (uint64_t)isr31, 0x08, 0x8E);
    SetGate(0x69, (uint64_t)isr0x69, 0x08, 0xEE /* Allow syscalls to be called from user mode*/, 0); // Syscall

    asm volatile("lidt %0;" ::"m"(idtPtr));

    outportb(0x20, 0x11);
    outportb(0xA0, 0x11);
    outportb(0x21, 0x20);
    outportb(0xA1, 0x28);
    outportb(0x21, 0x04);
    outportb(0xA1, 0x02);
    outportb(0x21, 0x01);
    outportb(0xA1, 0x01);
    outportb(0x21, 0x0);
    outportb(0xA1, 0x0);

    SetGate(32, (uint64_t)irq0, 0x08, 0x8E);
    SetGate(33, (uint64_t)irq1, 0x08, 0x8E);
    SetGate(34, (uint64_t)irq2, 0x08, 0x8E);
    SetGate(35, (uint64_t)irq3, 0x08, 0x8E);
    SetGate(36, (uint64_t)irq4, 0x08, 0x8E);
    SetGate(37, (uint64_t)irq5, 0x08, 0x8E);
    SetGate(38, (uint64_t)irq6, 0x08, 0x8E);
    SetGate(39, (uint64_t)irq7, 0x08, 0x8E);
    SetGate(40, (uint64_t)irq8, 0x08, 0x8E);
    SetGate(41, (uint64_t)irq9, 0x08, 0x8E);
    SetGate(42, (uint64_t)irq10, 0x08, 0x8E);
    SetGate(43, (uint64_t)irq11, 0x08, 0x8E);
    SetGate(44, (uint64_t)irq12, 0x08, 0x8E);
    SetGate(45, (uint64_t)irq13, 0x08, 0x8E);
    SetGate(46, (uint64_t)irq14, 0x08, 0x8E);
    SetGate(47, (uint64_t)irq15, 0x08, 0x8E);

    RegisterInterruptHandler(IPI_HALT, IPIHalt);
}

void RegisterInterruptHandler(uint8_t interrupt, isr_t handler, void* data) {
    interrupt_handlers[interrupt] = {.handler = handler, .data = data};
}

uint8_t ReserveUnusedInterrupt() {
    uint8_t interrupt = 0xFF;
    for (unsigned i = IRQ0 + 16 /* Ignore all legacy IRQs and exceptions */;
         i < 255 /* Ignore 0xFF */ && interrupt == 0xFF; i++) {
        if (!interrupt_handlers[i].handler) {
            interrupt_handlers[i].handler = InvalidInterruptHandler;
            interrupt = i;
        }
    }
    return interrupt;
}

void DisablePIC() {
    outportb(0x20, 0x11);
    outportb(0xA0, 0x11);
    outportb(0x21, 0xF0); // Remap IRQs on both PICs to 0xF0-0xF8
    outportb(0xA1, 0xF0);
    outportb(0x21, 0x04);
    outportb(0xA1, 0x02);
    outportb(0x21, 0x01);
    outportb(0xA1, 0x01);

    outportb(0x21, 0xFF); // Mask all interrupts
    outportb(0xA1, 0xFF);
}
} // namespace IDT

extern "C" void isr_handler(int intNum, RegisterContext* regs) {
    if (__builtin_expect(interrupt_handlers[intNum].handler != 0, 1)) {
        interrupt_handlers[intNum].handler(interrupt_handlers[intNum].data, regs);
    } else if (!(regs->ss & 0x3)) { // Check the CPL of the segment, caused by kernel?
        // Kernel Panic so tell other processors to stop executing
        APIC::Local::SendIPI(0, ICR_DSH_OTHER /* Send to all other processors except us */, ICR_MESSAGE_TYPE_FIXED,
                             IPI_HALT);

        IF_DEBUG(debugLevelSyscalls >= DebugLevelVerbose, { DumpLastSyscall(GetCPULocal()->currentThread); });
        Log::Error("Fatal Kernel Exception: ");
        Log::Info(intNum);
        Log::Info("RIP: ");
        Log::Info(regs->rip);
        Log::Info("Error Code: ");
        Log::Info(regs->err);
        Log::Info("Register Dump: a: ");
        Log::Write(regs->rax);
        Log::Write(", b:");
        Log::Write(regs->rbx);
        Log::Write(", c:");
        Log::Write(regs->rcx);
        Log::Write(", d:");
        Log::Write(regs->rdx);
        Log::Write(", S:");
        Log::Write(regs->rsi);
        Log::Write(", D:");
        Log::Write(regs->rdi);
        Log::Write(", sp:");
        Log::Write(regs->rsp);
        Log::Write(", bp:");
        Log::Write(regs->rbp);

        Log::Info("Stack Trace:");
        PrintStackTrace(regs->rbp);

        char temp[19];
        char temp2[19];
        const char* reasons[]{"Generic Exception",
                              "RIP: ",
                              itoa(regs->rip, temp, 16),
                              "Exception: ",
                              itoa(intNum, temp2, 16),
                              "Process:",
                              Scheduler::GetCurrentThread() ? Scheduler::GetCurrentThread()->parent->name : "none"};
        KernelPanic(reasons, 7);
        for (;;)
            ;
    } else {
        int res = acquireTestLock(&Scheduler::GetCurrentThread()->lock);
        assert(!res); // Make sure we acquired the lock

        Process* current = Process::Current();

        Log::Warning("Process %s crashed, PID: ", current->name);
        Log::Write(current->PID());
        Log::Write(", RIP: ");
        Log::Write(regs->rip);
        Log::Write(", Exception: ");
        Log::Write(intNum);
        Log::Info("Stack trace:");
        UserPrintStackTrace(regs->rbp, current->addressSpace);
        current->Die();
    }
}

extern "C" void irq_handler(int int_num, RegisterContext* regs) {
    LocalAPICEOI();

    if (__builtin_expect(interrupt_handlers[int_num].handler != 0, 1)) {
        ISRDataPair pair = interrupt_handlers[int_num];
        pair.handler(pair.data, regs);
    } else {
        Log::Warning("Unhandled IRQ: ");
        Log::Write(int_num);
    }
}

extern "C" void ipi_handler(int int_num, RegisterContext* regs) {
    LocalAPICEOI();

    if (__builtin_expect(interrupt_handlers[int_num].handler != 0, 1)) {
        ISRDataPair pair = interrupt_handlers[int_num];
        pair.handler(pair.data, regs);
    } else {
        Log::Warning("Unhandled IPI: ");
        Log::Write(int_num);
    }
}