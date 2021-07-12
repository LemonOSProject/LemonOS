#pragma once

#include <variant>

namespace Lemon::GUI {
using Variant = std::variant<std::string, long, int, const struct Surface*>;
}