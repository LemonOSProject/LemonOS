#include <CPU.h>
#include <CString.h>
#include <IOPorts.h>
#include <Lock.h>
#include <Serial.h>

namespace Serial {
enum Ports { COM1 = 0x3F8 };

#define PORT COM1

lock_t lock = 0;

void Initialize() {
    outportb(PORT + 1, 0x00); // Disable all interrupts
    outportb(PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outportb(PORT + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outportb(PORT + 1, 0x00); //                  (hi byte)
    outportb(PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outportb(PORT + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outportb(PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void Unlock() { releaseLock(&lock); }

void Write(const char c) {
    while (!(inportb(PORT + 5) & 0x20))
        ; // Make sure we are not transmitting data

    outportb(PORT, c);
}

void Write(const char* s) { Write(s, strlen(s)); }

void Write(const char* s, unsigned n) {
    if (CheckInterrupts()) {
        acquireLock(&lock); // Make the serial output readable
    }

    while (n--) {
        while (!(inportb(PORT + 5) & 0x20))
            ; // Make sure we are not transmitting data
        outportb(PORT, *s++);
    }

    if (CheckInterrupts()) {
        releaseLock(&lock);
    }
}
} // namespace Serial