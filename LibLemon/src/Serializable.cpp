#include <Lemon/Core/Serializable.h>

#include <Lemon/Graphics/Colour.h>

#include <cassert>
#include <iomanip>
#include <sstream>
#include <string>


template <> std::string Serialize(const std::string& value) {
    return value;
}

template <> std::string Deserialize(const std::string& value) {
    return value;
}

template <> std::string Serialize(const RGBAColour& colour) {
    std::stringstream ss;
    ss << "#" << std::hex << RGBAColour::ToARGB(colour);

    return ss.str();
}

template <> RGBAColour Deserialize(const std::string& value) {
    assert(value.front() == '#'); // Expect a hash out the front

    return RGBAColour::FromARGB(static_cast<uint32_t>(std::stoul(value, nullptr, 16)));
}
