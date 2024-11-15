#pragma once

#include <stddef.h>

#include "tables.h"

namespace hal {

constexpr auto hpet_main_counter = 0xf0;
constexpr auto hpet_timer0_base = 0x100;

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
    uint64_t comparator;
    uint64_t fsb_interrupt_route;
    uint64_t reserved;

    uint32_t cap_int_route() const {
        return (cap_config >> 32);
    }

    void set_int_route(uint8_t route) {
        cap_config |= (uint64_t)route << 9;
    }

    uint8_t get_int_route() const {
        return (cap_config >> 9) & 0x1f;
    }

    void set_periodic(int value) {
        cap_config = value ? (cap_config | 8) : (cap_config & ~8ull);
    }

    void set_int_enabled(int value) {
        cap_config = value ? (cap_config | 4) : (cap_config & ~4ull);
    }

    void set_level_triggered(int value) {
        cap_config = value ? (cap_config | 2) : (cap_config & ~2ull);
    }

    inline uint64_t comparator_value() const {
        return comparator;
    }
} __attribute__((packed));

struct HPETRegisters {
    alignas(16) uint64_t capabilities;
    alignas(16) uint64_t config;
    alignas(16) uint64_t interrupt_status;
    uint64_t reserved[25];
    alignas(16) uint64_t main_counter;

    HPETComparator *timer_n(uint8_t n) {
        return (HPETComparator *)((uintptr_t)this + hpet_timer0_base + n * sizeof(HPETComparator));
    }

    inline uint64_t counter_value() const {
        return main_counter;
    }

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

    inline int get_legacy_replacement() const {
        return config & 2;
    }

    inline void set_enable(int value) {
        // ENABLE_CNF
        if (value) {
            config |= 1;
        } else {
            config = config & ~1ull;
        }
    }
} __attribute__((packed));

static_assert(offsetof(HPETRegisters, main_counter) == 0xf0);

void init_hpet(HPETTable *hpet_table);

}
