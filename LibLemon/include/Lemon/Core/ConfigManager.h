#pragma once

#include <concepts>
#include <cassert>
#include <map>
#include <variant>
#include <type_traits>

#include <Lemon/Core/JSON.h>
#include <Lemon/Core/Serializable.h>

namespace Lemon {

template <typename T>
concept ConfigValueType = (std::same_as<T, std::string> || std::same_as<T, bool> || std::same_as<T, long> ||
                           std::same_as<T, unsigned long> || std::same_as<T, double>);

using ConfigValue = std::variant<std::string, bool, long, unsigned long, double>;

class ConfigManager {
public:
    ConfigManager() = default;
    ~ConfigManager() = default;

    template <Serializable T> inline void AddSerializedConfigProperty(const std::string& name, const T& defaultValue) {
        AddConfigProperty<std::string>(name, Serialize<T>(defaultValue));
    }

    template <ConfigValueType T> void AddConfigProperty(const std::string& name, T defaultValue){
        if constexpr (std::is_trivial<T>()){ // No need to move trivial types such as int, float, etc.
            m_entries[name] = defaultValue;
        } else {
            m_entries[name] = std::move(defaultValue);
        }
    }

    template <ConfigValueType T> inline T GetConfigProperty(const std::string& name) {
        auto& v = m_entries.at(name);
        assert(std::holds_alternative<T>(v));

        return std::get<T>(v);
    }

    template <Serializable T> inline T GetSerializedConfigProperty(const std::string& name) {
        return Deserialize<T>(std::get<std::string>(m_entries.at(name)));
    }

    void LoadJSONConfig(const std::string& path);

private:
    std::map<std::string, ConfigValue> m_entries;
};

} // namespace Lemon