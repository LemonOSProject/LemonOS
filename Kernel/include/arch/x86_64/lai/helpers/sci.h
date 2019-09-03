/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

#include <lai/core.h>

int lai_enable_acpi(uint32_t);
int lai_disable_acpi(void);

uint16_t lai_get_sci_event(void);
void lai_set_sci_event(uint16_t);

int lai_evaluate_sta(lai_nsnode_t *);
void lai_init_children(lai_nsnode_t *);