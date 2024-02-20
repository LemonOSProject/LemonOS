#include "panic.h"

#include "serial.h"

void lemon_panic(const char *reason) {
    serial::debug_write_string("panic: ");
    serial::debug_write_string(reason);
    serial::debug_write_string("\r\n");

    asm volatile("cli; hlt;");
}