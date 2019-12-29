/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

#include <lai/core.h>

#ifdef __cplusplus
extern "C"{
#endif

lai_api_error_t lai_enter_sleep(uint8_t);
lai_api_error_t lai_acpi_reset();

#ifdef __cplusplus
}
#endif