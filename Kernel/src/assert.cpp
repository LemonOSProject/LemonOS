#include <logging.h>

#include <panic.h>
#include <apic.h>
#include <serial.h>
#include <string.h>
#include <idt.h>
#include <strace.h>

extern "C"{

    [[noreturn]] void KernelAssertionFailed(const char* msg, const char* file, int line){
        asm("cli");

        APIC::Local::SendIPI(0, ICR_DSH_OTHER /* Send to all other processors except us */, ICR_MESSAGE_TYPE_FIXED, IPI_HALT);

        unlockSerial();
        Log::Error("Kernel Assertion Failed (%s) - file: %s, line: %d", msg, file, line);

        uint64_t rbp = 0;
        asm("mov %%rbp, %0" : "=r"(rbp));
        PrintStackTrace(rbp);

        char buf[16];
        itoa(line, buf, 10);

        const char* panic[] = {"Kernel Assertion Failed", msg, "File: ", file, "Line:", buf};
        KernelPanic(panic, 6);

        asm("hlt");
    }

}