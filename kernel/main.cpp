#include <tests/tests.h>

#include <thread/thread.h>
#include <clock.h>
#include <logging.h>

void kernel_test() {
    tests::run();

    thread::current()->kill();
}

void kernel_main() {
    assert(hal::cpu::interrupt_flag());
    log_info("hello from main");

    auto *test_thread = thread::create_kernel_thread("kernel_test", kernel_test, 0x10000, 0);

    for(;;) ;
}
