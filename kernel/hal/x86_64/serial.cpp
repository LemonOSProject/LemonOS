#include "serial.h"

#include "io_ports.h"

#define SERIAL_COM1 0x3F8

namespace serial {

enum Register {
    Data = 0,
    InterruptEnable = 1,
    LineControl = 3,
    ModemControl = 4,
    LineStatus = 5,
    ModemStatus = 6,
    Scratch = 7
};

void init() {
    io::out8(SERIAL_COM1 + Register::Data, 0);

    // Set baud rate divisor
    io::out8(SERIAL_COM1 + Register::LineControl, 0x80);
    
    // Divisor 0 (115200 baud)
    io::out8(SERIAL_COM1 + 0, 1);
    io::out8(SERIAL_COM1 + 1, 0);

    // 8 bit characters
    io::out8(SERIAL_COM1 + Register::LineControl, 0x3);

    // Disable interrupts
    io::out8(SERIAL_COM1 + Register::InterruptEnable, 0);
    io::out8(SERIAL_COM1 + Register::ModemControl, 0);
    io::out8(SERIAL_COM1 + Register::ModemControl, 0);
}

void debug_write_char(char c) {
    // Wait for the serial port to be ready
    while (!(io::in8(SERIAL_COM1 + Register::LineStatus) & 0x20));

    io::out8(SERIAL_COM1 + Register::Data, c);
}

void debug_write_string(const char *str) {
    while (*str) {
        debug_write_char(*(str++));
    }
}

}
