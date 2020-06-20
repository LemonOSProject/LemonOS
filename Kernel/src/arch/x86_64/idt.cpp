#include <idt.h>
#include <system.h>
#include <string.h>
#include <logging.h>
#include <hal.h>
#include <panic.h>
#include <scheduler.h>
#include <apic.h>

idt_entry_t idt[256];

idt_ptr_t idt_ptr;

isr_t interrupt_handlers[256];

extern "C"
void isr0();
extern "C"
void isr1();
extern "C"
void isr2();
extern "C"
void isr3();
extern "C"
void isr4();
extern "C"
void isr5();
extern "C"
void isr6();
extern "C"
void isr7();
extern "C"
void isr8();
extern "C"
void isr9();
extern "C"
void isr10();
extern "C"
void isr11();
extern "C"
void isr12();
extern "C"
void isr13();
extern "C"
void isr14();
extern "C"
void isr15();
extern "C"
void isr16();
extern "C"
void isr17();
extern "C"
void isr18();
extern "C"
void isr19();
extern "C"
void isr20();
extern "C"
void isr21();
extern "C"
void isr22();
extern "C"
void isr23();
extern "C"
void isr24();
extern "C"
void isr25();
extern "C"
void isr26();
extern "C"
void isr27();
extern "C"
void isr28();
extern "C"
void isr29();
extern "C"
void isr30();
extern "C"
void isr31();

extern "C"
void irq0();
extern "C"
void irq1();
extern "C"
void irq2();
extern "C"
void irq3();
extern "C"
void irq4();
extern "C"
void irq5();
extern "C"
void irq6();
extern "C"
void irq7();
extern "C"
void irq8();
extern "C"
void irq9();
extern "C"
void irq10();
extern "C"
void irq11();
extern "C"
void irq12();
extern "C"
void irq13();
extern "C"
void irq14();
extern "C"
void irq15();

extern "C"
void isr0x69();

extern "C"
void ipi0xFD(); // IPI_SCHEDULE
extern "C"
void ipi0xFE(); // IPI_HALT

extern "C"
void idt_flush();

int errCode = 0;

namespace IDT{
	void IPIHalt(regs64_t* r){
		//Log::Warning("Received halt IPI, halting processor.");

		asm("cli");
		asm("hlt");
	}

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
		idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
		idt_ptr.base = (uint64_t)&idt;

		for(int i = 0; i < 256; i++){
			SetGate(i, 0,0x08,0x8E);
		}

		SetGate(0, (uint64_t)isr0,0x08,0x8E);
		SetGate(1, (uint64_t)isr1,0x08,0x8E);
		SetGate(2, (uint64_t)isr2,0x08,0x8E);
		SetGate(3, (uint64_t)isr3,0x08,0x8E);
		SetGate(4, (uint64_t)isr4,0x08,0x8E);
		SetGate(5, (uint64_t)isr5,0x08,0x8E);
		SetGate(6, (uint64_t)isr6,0x08,0x8E);
		SetGate(7, (uint64_t)isr7,0x08,0x8E);
		SetGate(8, (uint64_t)isr8,0x08,0x8E, 2); // Double Fault
		SetGate(9, (uint64_t)isr9,0x08,0x8E);
		SetGate(10, (uint64_t)isr10,0x08,0x8E);
		SetGate(11, (uint64_t)isr11,0x08,0x8E);
		SetGate(12, (uint64_t)isr12,0x08,0x8E);
		SetGate(13, (uint64_t)isr13,0x08,0x8E);
		SetGate(14, (uint64_t)isr14,0x08,0x8E);
		SetGate(15, (uint64_t)isr15,0x08,0x8E);
		SetGate(16, (uint64_t)isr16,0x08,0x8E);
		SetGate(17, (uint64_t)isr17,0x08,0x8E);
		SetGate(18, (uint64_t)isr18,0x08,0x8E);
		SetGate(19, (uint64_t)isr19,0x08,0x8E);
		SetGate(20, (uint64_t)isr20,0x08,0x8E);
		SetGate(21, (uint64_t)isr21,0x08,0x8E);
		SetGate(22, (uint64_t)isr22,0x08,0x8E);
		SetGate(23, (uint64_t)isr23,0x08,0x8E);
		SetGate(24, (uint64_t)isr24,0x08,0x8E);
		SetGate(25, (uint64_t)isr25,0x08,0x8E);
		SetGate(26, (uint64_t)isr26,0x08,0x8E);
		SetGate(27, (uint64_t)isr27,0x08,0x8E);
		SetGate(28, (uint64_t)isr28,0x08,0x8E);
		SetGate(29, (uint64_t)isr29,0x08,0x8E);
		SetGate(30, (uint64_t)isr30,0x08,0x8E);
		SetGate(31, (uint64_t)isr31,0x08,0x8E);
		SetGate(0x69, (uint64_t)isr0x69, 0x08, 0xEE /* Allow syscalls to be called from user mode*/, 0); // Syscall
		SetGate(IPI_SCHEDULE, (uint64_t)ipi0xFD,0x08,0x8E);
		SetGate(IPI_HALT, (uint64_t)ipi0xFE,0x08,0x8E);

		idt_flush();

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
		
		__asm__ __volatile__("sti");

		RegisterInterruptHandler(IPI_HALT, IPIHalt);
	}	

	void RegisterInterruptHandler(uint8_t interrupt, isr_t handler) {
		interrupt_handlers[interrupt] = handler;
	}

	void DisablePIC(){
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

	int GetErrCode(){
		return errCode;
	}
}

extern "C"
	void isr_handler(int int_num, regs64_t* regs, int err_code) {
		errCode = err_code;
		if (interrupt_handlers[int_num] != 0) {
			interrupt_handlers[int_num](regs);
		} else if(int_num == 0x69){
			Log::Warning("\r\nEarly syscall");
		} else if(!(regs->ss & 0x3)){ // Check the CPL of the segment, caused by kernel?
			// Kernel Panic so tell other processors to stop executing
			APIC::Local::SendIPI(0, ICR_DSH_OTHER /* Send to all other processors except us */, ICR_MESSAGE_TYPE_FIXED, IPI_HALT);

			Log::Error("Fatal Exception: ");
			Log::Info(int_num);
			Log::Info("RIP: ");
			Log::Info(regs->rip);
			Log::Info("Error Code: ");
			Log::Info(err_code);
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
			
			uint64_t* stack = (uint64_t*)regs->rbp;
			
			while(stack){
				uint64_t* rbp = (uint64_t*)(*stack);
				uint64_t rip = *(stack + 1);
				Log::Info(rip);
				stack = rbp;
			}

			char temp[16];
			char temp2[16];
			char temp3[16];
			const char* reasons[]{"Generic Exception","RIP: ", itoa(regs->rip, temp, 16),"Exception: ",itoa(int_num, temp2, 16), "Process:", itoa(Scheduler::GetCurrentProcess() ? (Scheduler::GetCurrentProcess()->pid) : 0,temp3,10)};;
			KernelPanic(reasons, 7);
			for (;;);
		} else {
			Log::Warning("Process crashed, PID: ");
			Log::Write(Scheduler::GetCurrentProcess()->pid);
			Log::Write(", RIP: ");
			Log::Write(regs->rip);
			Log::Write(", Exception: ");
			Log::Write(int_num);
			Scheduler::EndProcess(Scheduler::GetCurrentProcess());
		}
	}

	extern "C"
	void irq_handler(int int_num, regs64_t* regs) {
		LocalAPICEOI();
		
		if (__builtin_expect(interrupt_handlers[int_num] != 0, 1)) {
			isr_t handler;
			handler = interrupt_handlers[int_num];
			handler(regs);
		} else {
			Log::Warning("Unhandled IRQ: ");
			Log::Write(int_num);
		}
	}
	
	extern "C"
	void ipi_handler(int int_num, regs64_t* regs) {
		LocalAPICEOI();

		if (__builtin_expect(interrupt_handlers[int_num] != 0, 1)) {
			isr_t handler;
			handler = interrupt_handlers[int_num];
			handler(regs);
		} else {
			Log::Warning("Unhandled IPI: ");
			Log::Write(int_num);
		}
	}