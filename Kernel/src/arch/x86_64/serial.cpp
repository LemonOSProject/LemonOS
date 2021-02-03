#include <system.h>
#include <serial.h>
#include <string.h>
#include <lock.h>
#include <cpu.h>

#define PORT 0x3F8 // COM 1

void initialize_serial() {
	outportb(PORT + 1, 0x00);    // Disable all interrupts
	outportb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
	outportb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
	outportb(PORT + 1, 0x00);    //                  (hi byte)
	outportb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
	outportb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
	outportb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int is_transmit_empty() {
	return inportb(PORT + 5) & 0x20;
}

volatile int lock = 0;

void unlockSerial(){
	lock = 0;
}

void write_serial(const char c) {
	while (is_transmit_empty() == 0);

	outportb(PORT, c);
}

void write_serial(const char* s) {
	write_serial_n(s, strlen(s));
}

void write_serial_n(const char* s, unsigned n) {
	unsigned i = 0;

	if(CheckInterrupts())
		acquireLock(&lock); // Make the serial output readable
	while (i++ < n){
		while(is_transmit_empty() == 0);
		outportb(PORT, *s++);
	}
	if(CheckInterrupts())
		releaseLock(&lock);
}