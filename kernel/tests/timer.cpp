#include <thread/timer.h>

#include "logging.h"

namespace tests {

int run_timer() {
    auto &queue = global_timer_queue();
    auto *clk = get_best_clock_device();

    bool timer1_expired = false;
    bool timer2_expired = false;

    auto cb = [&timer1_expired, clk]() {
        log_info("Timer expired {} ns", clk->ns_since_boot());

        timer1_expired = true;
    };

    auto cb2 = [&timer2_expired]() {
        log_info("Timer 2 expired!");

        timer2_expired = true;
    };

    auto timer2 = queue.add_timer(Callback<>::create(&cb2), 60'000'000);
    auto timer1 = queue.add_timer(Callback<>::create(&cb), 50'000'000);

    assert(timer1 && timer2);

    // 2ms leeway, wait a max of 52ms
    auto deadline = clk->ns_since_boot() + 52'000'000;
    // 62ms
    auto deadline2 = clk->ns_since_boot() + 62'000'000;

    log_info("ns_since_boot: {} ns deadline {} ns", clk->ns_since_boot(), deadline);

    while (!timer1_expired && clk->ns_since_boot() < deadline)
        ;

    log_info("ns_since_boot: {} ns", clk->ns_since_boot());

    assert(timer1_expired);

    while (!timer2_expired && clk->ns_since_boot() < deadline2)
        ;

    log_info("ns_since_boot: {} ns", clk->ns_since_boot());

    assert(timer2_expired);

    return 0;
}

} // namespace tests
