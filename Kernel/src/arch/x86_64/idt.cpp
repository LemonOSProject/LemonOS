#include <idt.h>
#include <system.h>
#include <string.h>
#include <logging.h>
#include <hal.h>
#include <panic.h>
#include <scheduler.h>

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
void idt_flush();

int errCode = 0;

namespace IDT{
	static void SetGate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
		idt[num].base_high = (base >> 32);
		idt[num].base_med = (base >> 16) & 0xFFFF;
		idt[num].base_low = base & 0xFFFF;

		idt[num].sel = sel;
		idt[num].null = 0;
		idt[num].ist = 0;

		idt[num].flags = flags;
	}

	void Initialize() {
		idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
		idt_ptr.base = (uint64_t)&idt;

		memset((uint8_t*)&idt, 0, sizeof(idt_entry_t) * 256);
		memset((uint8_t*)&interrupt_handlers, 0, sizeof(isr_t) * 256);
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
		SetGate(8, (uint64_t)isr8,0x08,0x8E);
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
		SetGate(0x69, (uint64_t)isr0x69, 0x08, 0xEE /* Allow syscalls to be called from user mode*/);

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

		memset((uint8_t*)&interrupt_handlers, 0, sizeof(isr_t) * 256);
		__asm__ __volatile__("sti");
	}	

	void RegisterInterruptHandler(uint8_t interrupt, isr_t handler) {
		interrupt_handlers[interrupt] = handler;
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
		}
		else {
			Log::Error("Fatal Exception: ");
			Log::Info(int_num);
			Log::Info("EIP: ");
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
			/*
			uint8_t xmm_x[16];
			asm("movdqu %%xmm0, (%0)" : "=r"(xmm_x));
			Log::Write("\r\nxmm0: {");
			for(int i = 15; i >= 0; i--){
				Log::Write(xmm_x[i]);
				Log::Write(", ");
			}
			asm("movdqu %%xmm1, (%0)" : "=r"(xmm_x));
			Log::Write("}, xmm1: {");
			for(int i = 15; i >= 0; i--){
				Log::Write(xmm_x[i]);
				Log::Write(", ");
			}
			asm("movdqu %%xmm2, (%0)" : "=r"(xmm_x));
			Log::Write("}, xmm3: {");
			for(int i = 15; i >= 0; i--){
				Log::Write(xmm_x[i]);
				Log::Write(", ");
			}
			asm("movdqu %%xmm3, (%0)" : "=r"(xmm_x));
			Log::Write("}, xmm3: {");
			for(int i = 15; i >= 0; i--){
				Log::Write(xmm_x[i]);
				Log::Write(", ");
			}
			asm("movdqu %%xmm4, (%0)" : "=r"(xmm_x));
			Log::Write("}, xmx4: {");
			for(int i = 15; i >= 0; i--){
				Log::Write(xmm_x[i]);
				Log::Write(", ");
			}


			char temp[16];
			char temp2[16];
			char temp3[16];
			const char* reasons[]{"Generic Exception","RIP: ", itoa(regs->rip, temp, 16),"Exception: ",itoa(int_num, temp2, 16), "Process:", itoa(Scheduler::GetCurrentProcess() ? (Scheduler::GetCurrentProcess()->pid) : 0,temp3,10)};;
			KernelPanic(reasons, 7);*/
			for (;;);
		}
	}

	extern "C"
	void irq_handler(int int_num, regs64_t* regs) {
		if (int_num >= 40) {
			outportb(0xA0, 0x20);
		}

		outportb(0x20, 0x20);
		
		if (interrupt_handlers[int_num] != 0) {
			isr_t handler;
			handler = interrupt_handlers[int_num];
			handler(regs);
		}
	}