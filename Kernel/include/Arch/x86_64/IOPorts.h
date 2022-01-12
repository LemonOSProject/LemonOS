#pragma once

#include <stdint.h>

void outportb(uint16_t port, uint8_t value);
void outportw(uint16_t port, uint16_t value);
void outportd(uint16_t port, uint32_t value);
void outportl(uint16_t port, uint32_t value);

uint8_t inportb(uint16_t port);
uint16_t inportw(uint16_t port);
uint32_t inportd(uint16_t port);
uint32_t inportl(uint16_t port);
