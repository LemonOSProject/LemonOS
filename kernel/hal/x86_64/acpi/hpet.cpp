#include "hpet.h"

#include "clock.h"
#include "vmem.h"
#include "irq.h"

#include <assert.h>
#include <logging.h>

namespace hal {

HPETRegisters *hpet_registers;
class HPETTimerDevice *clock_device;

uint64_t counter_clk_period;

void hpet_irq(void *data, cpu::InterruptFrame *frame);

inline uint64_t hpet_ticks_to_ns(uint64_t ticks) {
    return ticks * counter_clk_period / 1000'000;
}

inline uint64_t hpet_ns_to_ticks(uint64_t ns) {
    return (ns * 1000'000 + counter_clk_period - 1) / hpet_registers->counter_clk_period();
}

class HPETTimerDevice : public ClockDevice {
public:
    void start_timer(uint64_t ns) override {
        cpu::InterruptDisabler disable_ints;

        auto *timer0 = hpet_registers->timer_n(0);

        auto deadline = hpet_ns_to_ticks(ns);

        timer0->set_int_enabled(1);
        timer0->comparator = deadline;

        timer_is_running = true;
    }

    void stop_timer() override {
        cpu::InterruptDisabler disable_ints;

        timer_is_running = false;
    }

    uint64_t ns_since_boot() override {
        return hpet_ticks_to_ns(hpet_registers->main_counter);
    }

    const char* name() const override {
        return "hpet_timer0";
    }

    bool timer_is_running = false;
};

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
    log_info("hpet: HPET period: {}.{} ns", hpet_registers->counter_clk_period() / 1000'000,
        hpet_registers->counter_clk_period() % 1000'000);
    log_info("hpet: HPET number of comparators: {}", hpet_table->comparator_count);
    log_info("hpet: HPET counter size: {}", hpet_table->counter_size);
    log_info("hpet: HPET min tick: {}", hpet_table->min_tick);
    log_info("hpet: HPET page protection: {}", hpet_table->page_protection);

    // address of hpet config reg
    log_info("hpet: HPET cap reg: {:x}", &hpet_registers->capabilities);
    log_info("hpet: HPET config reg: {:x}", &hpet_registers->config);
    log_info("hpet: HPET main counter: {:x}", &hpet_registers->main_counter);

    log_info("hpet: frequency {}KHz", 1000000000000 / hpet_registers->counter_clk_period());

    counter_clk_period = hpet_registers->counter_clk_period();

    hpet_registers->set_enable(0);

    hpet_registers->interrupt_status = 0;

    auto num_timers = hpet_registers->cap_num_timers();
    assert(num_timers >= 3);

    // Make sure we've mapped enough memory for the timers
    assert(num_timers * sizeof(HPETComparator) + hpet_timer0_base <= PAGE_SIZE_4K);

    for (int i = 0; i < num_timers; i++) {
        auto *timer = hpet_registers->timer_n(i);

        timer->set_int_enabled(0);

        log_info("hpet: Timer {} capabilities: {:x}", i, timer->cap_config);
        log_info("\t -> comparator value: {:x}", timer->comparator_value());
        log_info("\t -> valid interrupt routes: {:x}", timer->cap_int_route());
    }

    // Configure timer 0
    auto *timer0 = hpet_registers->timer_n(0);

    auto valid_routes = timer0->cap_int_route();
    cpu::GSI *timer0_gsi = nullptr;

    if (!hpet_registers->cap_legacy_replacement()) {
        log_fatal("hpet: HPET does not support legacy replacement");
        return;
    }

    hpet_registers->set_legacy_replacement(1);

    for (int i = 0; i < 8; i++, valid_routes >>= 1) {
        if (!(valid_routes & 1)) {
            continue;
        }

        auto *gsi = cpu::get_gsi(i);
        if (!gsi) {
            continue;
        }

        if (gsi->is_in_use()) {
            if (gsi->is_iso && gsi->legacy_irq == cpu::LEGACY_IRQ_PIT) {
                timer0_gsi = gsi;
                break;
            }
        }
    }

    if (!timer0_gsi) {
        log_fatal("hpet: No valid GSI for timer 0");
        return;
    }

    log_info("hpet: Using GSI {:x} for timer 0", timer0_gsi->gsi);
    
    bool hpet_can_recieve_interrupts = false;
    auto hpet_test_handler = [&](cpu::InterruptFrame*) {
        log_info("hpet interrupt!");
        hpet_can_recieve_interrupts = true;

        cpu::local_apic_eoi();
    };

    auto timer0_vector = cpu::TIMER_IRQ_VECTOR;

    timer0->set_int_route(timer0_gsi->gsi);
    assert(timer0->get_int_route() == timer0_gsi->gsi);

    timer0->set_level_triggered(0);
    timer0->set_periodic(0);
    timer0->set_int_enabled(1);

    install_irq_handler(timer0_vector, Callback<cpu::InterruptFrame*>::create(&hpet_test_handler));
    timer0_gsi->redirect(timer0_vector);

    hpet_registers->main_counter = 0;
    hpet_registers->set_enable(1);

    auto one_ms = hpet_ns_to_ticks(1'000'000);
    auto deadline = hpet_registers->counter_value() + one_ms * 4;

    timer0->comparator = deadline;

    asm("sti");

    while (!hpet_can_recieve_interrupts 
            && hpet_registers->counter_value() < deadline + one_ms * 50) {
        asm("pause");
    }

    asm("cli");

    timer0->set_int_enabled(0);

    if (!hpet_can_recieve_interrupts) {
        lemon_panic("hpet not firing interrupts");
    }

    clock_device = new HPETTimerDevice;
    add_clock_device(clock_device);

    install_irq_handler(cpu::TIMER_IRQ_VECTOR, Callback<cpu::InterruptFrame*>{
        .fn = &hpet_irq,
        .data = clock_device
    });
}

void hpet_irq(void *data, cpu::InterruptFrame *frame) {
    auto &cb = clock_device->timer_callback;

    cpu::local_apic_eoi();

    if (cb) {
        cb.call();
    }
}

}
