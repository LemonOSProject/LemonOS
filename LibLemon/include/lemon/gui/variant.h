#include <variant>

namespace Lemon::GUI {
    using Variant = std::variant<std::string, long, int, struct Surface>;
}