/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#include <lai/core.h>
#include "aml_opcodes.h"
#include "ns_impl.h"
#include "libc.h"
#include "exec_impl.h"
#include "eval.h"

uint64_t bswap64(uint64_t);
uint32_t bswap32(uint32_t);
uint16_t bswap16(uint16_t);
uint8_t char_to_hex(char);

int lai_is_name(char character) {
    if ((character >= '0' && character <= '9')
        || (character >= 'A' && character <= 'Z')
        || character == '_'
        || character == ROOT_CHAR
        || character == PARENT_CHAR
        || character == MULTI_PREFIX
        || character == DUAL_PREFIX)
        return 1;

    else
        return 0;
}

uint16_t bswap16(uint16_t word) {
    return (uint16_t)((word >> 8) & 0xFF) | ((word << 8) & 0xFF00);
}

uint32_t bswap32(uint32_t dword) {
    return (uint32_t)((dword>>24) & 0xFF)
        | ((dword<<8) & 0xFF0000)
        | ((dword>>8)&0xFF00)
        | ((dword<<24)&0xFF000000);
}

uint64_t bswap64(uint64_t qword) {
    return (uint64_t)(
          (qword & ((uint64_t)0xff << 56)) >> 56
        | (qword & ((uint64_t)0xff << 48)) >> 40
        | (qword & ((uint64_t)0xff << 40)) >> 24
        | (qword & ((uint64_t)0xff << 32)) >> 8
        | (qword & ((uint64_t)0xff << 24)) << 8
        | (qword & ((uint64_t)0xff << 16)) << 24
        | (qword & ((uint64_t)0xff <<  8)) << 40
        | (qword & ((uint64_t)0xff <<  0)) << 56
    );
}

uint8_t char_to_hex(char character) {
    if (character <= '9')
        return character - '0';
    else if(character >= 'A' && character <= 'F')
        return character - 'A' + 10;
    else if(character >= 'a' && character <= 'f')
        return character - 'a' + 10;

    return 0;
}

void lai_eisaid(lai_variable_t *object, char *id) {
    size_t n = lai_strlen(id);
    if (lai_strlen(id) != 7) {
        if(lai_create_string(object, n))
            lai_panic("could not allocate memory for string");
        memcpy(lai_exec_string_access(object), id, n);
        return;
    }

    // convert a string in the format "UUUXXXX" to an integer
    // "U" is an ASCII character, and "X" is an ASCII hex digit
    object->type = LAI_INTEGER;

    uint32_t out = 0;
    out |= ((id[0] - 0x40) << 26);
    out |= ((id[1] - 0x40) << 21);
    out |= ((id[2] - 0x40) << 16);
    out |= char_to_hex(id[3]) << 12;
    out |= char_to_hex(id[4]) << 8;
    out |= char_to_hex(id[5]) << 4;
    out |= char_to_hex(id[6]);

    out = bswap32(out);
    object->integer = (uint64_t)out & 0xFFFFFFFF;
}
