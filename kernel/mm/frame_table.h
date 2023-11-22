#pragma once

#include <stdint.h>
#include <stddef.h>

namespace frame_allocator {

uint64_t alloc();
void free(uint64_t frame);

}
