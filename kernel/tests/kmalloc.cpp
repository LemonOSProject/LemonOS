#include "tests.h"

#include <logging.h>
#include <mm/kmalloc.h>

namespace tests {

void run_kmalloc() {
    log_info("Testing kmalloc...");

    void *p[5];

    // This test is definitely implementation dependant,
    // but lets check holes created by kfree() get filled in
    p[0] = mm::kmalloc(128);
    p[1] = mm::kmalloc(128);
    p[2] = mm::kmalloc(128);
    p[3] = mm::kmalloc(128);

    mm::kfree(p[1]);

    p[4] = mm::kmalloc(128);

    assert(p[4] == p[1]);

    mm::kfree(p[0]);
    mm::kfree(p[2]);
    mm::kfree(p[3]);
    mm::kfree(p[4]);

    // Non-power of two
    const int num_sizes = 6;
    size_t sizes[num_sizes] = {9, 127, 65, 54, 240, 150};

    log_info("Testing kmalloc with non-powers of 2...");
    const int num_allocations = 512;
    for (int i = 0; i < num_sizes; i++) {
        size_t sz = sizes[i];

        log_info("{} bytes", sz);

        void *ptrs[num_allocations];
        for (int j = 0; j < num_allocations; j++) {
            ptrs[j] = mm::kmalloc(sz);
        }

        int chunks_left = num_allocations;

        // Free chunks in a (not-so) random order
        size_t to_free = 7;
        while (chunks_left > 0) {
            if (ptrs[to_free % num_allocations]) {
                mm::kfree(ptrs[to_free % num_allocations]);
                chunks_left--;

                ptrs[to_free % num_allocations] = 0;
            }

            to_free += 1;
        }
    }
    log_info("done!");
}

} // namespace tests
