/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

#include <lai/core.h>

__attribute__((deprecated("use lai_pci_route_pin instead")))
int lai_pci_route(acpi_resource_t *, uint16_t, uint8_t, uint8_t, uint8_t);
lai_api_error_t lai_pci_route_pin(acpi_resource_t *, uint16_t, uint8_t, uint8_t, uint8_t, uint8_t);

lai_nsnode_t *lai_pci_find_device(lai_nsnode_t *, uint8_t, uint8_t, lai_state_t *);
lai_nsnode_t *lai_pci_find_bus(uint16_t, uint8_t, lai_state_t *);

struct lai_prt_iterator {
    size_t i;
    lai_variable_t *prt;

    int slot, function;
    uint8_t pin;
    lai_nsnode_t *link;
    size_t resource_idx;
    uint32_t gsi;
    uint32_t flags;
};

#define LAI_PRT_ITERATOR_INITIALIZER(prt) {0, prt, 0, 0, 0, NULL, 0, 0, 0}

lai_api_error_t lai_pci_parse_prt(struct lai_prt_iterator *iter);
