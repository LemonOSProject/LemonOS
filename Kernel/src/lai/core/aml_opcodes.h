
/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

#define ZERO_OP				0x00
#define ONE_OP				0x01
#define ALIAS_OP			0x06
#define NAME_OP				0x08
#define BYTEPREFIX			0x0A
#define WORDPREFIX			0x0B
#define DWORDPREFIX			0x0C
#define STRINGPREFIX			0x0D
#define QWORDPREFIX			0x0E
#define SCOPE_OP			0x10
#define BUFFER_OP			0x11
#define PACKAGE_OP			0x12
#define VARPACKAGE_OP			0x13
#define METHOD_OP			0x14
#define DUAL_PREFIX			0x2E
#define MULTI_PREFIX			0x2F
#define EXTOP_PREFIX			0x5B
#define ROOT_CHAR			0x5C
#define PARENT_CHAR			0x5E
#define LOCAL0_OP			0x60
#define LOCAL1_OP			0x61
#define LOCAL2_OP			0x62
#define LOCAL3_OP			0x63
#define LOCAL4_OP			0x64
#define LOCAL5_OP			0x65
#define LOCAL6_OP			0x66
#define LOCAL7_OP			0x67
#define ARG0_OP				0x68
#define ARG1_OP				0x69
#define ARG2_OP				0x6A
#define ARG3_OP				0x6B
#define ARG4_OP				0x6C
#define ARG5_OP				0x6D
#define ARG6_OP				0x6E
#define STORE_OP			0x70
#define REFOF_OP			0x71
#define ADD_OP				0x72
#define SUBTRACT_OP			0x74
#define INCREMENT_OP			0x75
#define DECREMENT_OP			0x76
#define MULTIPLY_OP			0x77
#define DIVIDE_OP			0x78
#define SHL_OP				0x79
#define SHR_OP				0x7A
#define AND_OP				0x7B
#define OR_OP				0x7D
#define XOR_OP				0x7F
#define NOT_OP				0x80
#define DEREF_OP			0x83
#define SIZEOF_OP			0x87
#define INDEX_OP			0x88
#define DWORDFIELD_OP			0x8A
#define WORDFIELD_OP			0x8B
#define BYTEFIELD_OP			0x8C
#define BITFIELD_OP			0x8D
#define QWORDFIELD_OP			0x8F
#define LAND_OP				0x90
#define LOR_OP				0x91
#define LNOT_OP				0x92
#define LEQUAL_OP			0x93
#define LGREATER_OP			0x94
#define LLESS_OP			0x95
#define TOBUFFER_OP         0x96
#define TODECIMALSTRING_OP  0x97
#define TOHEXSTRING_OP      0x98
#define TOINTEGER_OP        0x99
#define TOSTRING_OP         0x9C
#define COPYOBJECT_OP		0x9D
#define CONTINUE_OP			0x9F
#define IF_OP				0xA0
#define ELSE_OP				0xA1
#define WHILE_OP			0xA2
#define NOP_OP				0xA3
#define RETURN_OP			0xA4
#define BREAK_OP			0xA5
#define BREAKPOINT_OP       0xCC
#define ONES_OP				0xFF

// Extended opcodes
#define MUTEX				0x01
#define EVENT               0x02
#define CONDREF_OP			0x12
#define ARBFIELD_OP			0x13
#define SLEEP_OP			0x22
#define ACQUIRE_OP			0x23
#define RELEASE_OP			0x27
#define FROM_BCD_OP         0x28
#define TO_BCD_OP           0x29
#define DEBUG_OP			0x31
#define FATAL_OP            0x32
#define OPREGION			0x80
#define FIELD				0x81
#define DEVICE				0x82
#define PROCESSOR			0x83
#define POWER_RES           0x84
#define THERMALZONE			0x85
#define INDEXFIELD			0x86	// ACPI spec v5.0 section 19.5.60
#define BANKFIELD			0x87

// Field Access Type
#define FIELD_ANY_ACCESS		0x00
#define FIELD_BYTE_ACCESS		0x01
#define FIELD_WORD_ACCESS		0x02
#define FIELD_DWORD_ACCESS		0x03
#define FIELD_QWORD_ACCESS		0x04
#define FIELD_LOCK			0x10
#define FIELD_PRESERVE			0x00
#define FIELD_WRITE_ONES		0x01
#define FIELD_WRITE_ZEROES		0x02

// Methods
#define METHOD_ARGC_MASK		0x07
#define METHOD_SERIALIZED		0x08
