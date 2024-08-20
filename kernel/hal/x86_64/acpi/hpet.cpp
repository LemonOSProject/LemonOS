#include "hpet.h"

#include "vmem.h"

#include <assert.h>
#include <logging.h>

namespace hal {

HPETRegisters *hpet_registers;

void init_hpet(HPETTable *hpet_table) {
    log_info("hpet: Found HPET table at {:x}", hpet_table);

    hpet_registers = (HPETRegisters *)create_io_mapping(hpet_table->address, PAGE_SIZE_4K, mm::MemoryProtection::rw());
    assert(hpet_registers);

    log_info("hpet: HPET address: {:x}", hpet_table->address);
    log_info("hpet: HPET mapping {}", hpet_table->address_space_id);
    log_info("hpet: HPET number: {}", hpet_table->hpet_number);
    log_info("hpet: HPET vendor ID: {:x}", hpet_registers->vendor_id());
    log_info("hpet: HPET legacy replacement: {}", hpet_registers->cap_legacy_replacement());
    log_info("hpet: HPET count size: {}", hpet_registers->cap_count_size());
    log_info("hpet: HPET num timers: {}", hpet_registers->cap_num_timers());
    log_info("hpet: HPET number of comparators: {}", hpet_table->comparator_count);
    log_info("hpet: HPET counter size: {}", hpet_table->counter_size);
    log_info("hpet: HPET min tick: {}", hpet_table->min_tick);
    log_info("hpet: HPET page protection: {}", hpet_table->page_protection);

    log_info("hpet: frequency {}KHz", 1000000000000 / hpet_registers->counter_clk_period());

    assert(hpet_registers->cap_num_timers() >= 3);
}

}
