#pragma once

namespace hal::cpu {
    struct InterruptFrame;
}

void lemon_panic(const char *reason, hal::cpu::InterruptFrame *frame = nullptr);