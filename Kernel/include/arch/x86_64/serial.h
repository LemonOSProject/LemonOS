#pragma once

void initialize_serial();
void unlockSerial();
void write_serial(const char c);
void write_serial(const char* s);
void write_serial_n(const char* s, unsigned n);
void write_serial_string(const char* s);