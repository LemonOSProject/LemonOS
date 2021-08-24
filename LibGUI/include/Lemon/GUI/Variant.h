#pragma once

#include <variant>

struct Surface;

namespace Lemon::GUI {
using Variant = std::variant<std::string, long, int, const Surface*>;
}