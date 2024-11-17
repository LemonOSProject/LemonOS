#include "tests.h"

namespace tests {

void run_kmalloc();
void run_timer();

void run() {
    run_kmalloc();
    run_timer();
}

} // namespace tests
