#pragma once

#include "tables.h"

namespace hal {

struct HPETTable {
    acpi_header_t header;

    uint32_t hardware_rev : 8;
    uint32_t comparator_count : 5;
    uint32_t counter_size : 1;
    uint32_t : 1;
    uint32_t legacy_replacement : 1;
    uint32_t pci_vendor_id : 16;

    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;

    uint64_t address;

    uint8_t hpet_number;
    uint16_t min_tick;
    uint8_t page_protection;
} __attribute__((packed));

struct HPETComparator {
    uint64_t cap_config;
    uint64_t comparator_value;
    uint64_t interrupt_route;
    uint64_t reserved;
} __attribute__((packed));

struct HPETRegisters {
    uint64_t capabilities;
    uint64_t config;
    uint64_t interrupt_status;
    uint64_t main_counter;
    HPETComparator comparators[];

    inline uint64_t counter_clk_period() const {
        return (capabilities >> 32);
    } 

    inline uint16_t vendor_id() const {
        return (capabilities >> 16) & 0xffff;
    }

    // HPET is capable of legacy replacement
    inline int cap_legacy_replacement() const {
        return (capabilities & (1 << 15));
    }

    // COUNT_SIZE_CAP, If 1, HPET can operate with a 64-bit count
    inline int cap_count_size() const {
        return (capabilities & (1 << 13));
    }

    // Get the number of timers
    inline int cap_num_timers() const {
        // Get NUM_TIM_CAP, Number of timers - 1,
        // add 1 to get number of timers
        return ((capabilities >> 8) & 0x1f) + 1;
    }

    inline void set_legacy_replacement(int value) {
        // LEG_RT_CNF
        if (value) {
            config |= 2;
        } else {
            config = config & ~(2ull);
        }
    }
} __attribute__((packed));

void init_hpet(HPETTable *hpet_table);

}
