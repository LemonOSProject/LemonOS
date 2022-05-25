#include <Lemon/Core/ConfigManager.h>

#include <Lemon/Core/Logger.h>
#include <Lemon/Core/JSON.h>

#include <functional>

namespace Lemon {

void ConfigManager::LoadJSONConfig(const std::string& path) {
    JSONParser parser(path.c_str());

    std::function<void(const std::string&, JSONValue&)> readObject;
    readObject = [this, &readObject](const std::string& configPrefix, JSONValue& object) -> void {
        assert(object.IsObject());
        auto& obj = *object.object;
        for (auto& val : obj) {
            if (val.second.IsObject()) {
                // We add the key to the prefix.
                // Config keys will look like this
                //     object.subobject.key
                readObject(configPrefix + val.first + ".", val.second);
            } else if(auto it = m_entries.find(configPrefix + val.first); it != m_entries.end()) {
                ConfigValue& configEntry = it->second; // Make sure the config entry exists
                if(std::holds_alternative<long>(configEntry)){
                    configEntry = val.second.AsSignedNumber();
                } else if(std::holds_alternative<unsigned long>(configEntry)){
                    configEntry = val.second.AsUnsignedNumber();
                } else if(std::holds_alternative<bool>(configEntry)){
                    configEntry = val.second.AsBool();
                } else if(std::holds_alternative<std::string>(configEntry)){
                    configEntry = val.second.AsString();
                }
            }
        }
    };

    auto root = parser.Parse();
    if(!root.IsObject()){
        Logger::Warning("[ConfigManager] Failed to laod JSON config at {}", path);
        return;
    }

    readObject("", root);
}

} // namespace Lemon