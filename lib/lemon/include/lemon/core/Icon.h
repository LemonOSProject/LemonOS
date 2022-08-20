#pragma once

#include <lemon/graphics/Surface.h>

namespace Lemon {
struct Icon {
    Surface icon16{.width = 0, .height = 0, .depth = 32, .buffer = nullptr};
    Surface icon32{.width = 0, .height = 0, .depth = 32, .buffer = nullptr};
    Surface icon64{.width = 0, .height = 0, .depth = 32, .buffer = nullptr};
};
} // namespace Lemon
