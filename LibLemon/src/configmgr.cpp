#include <lemon/core/configmgr.h>

namespace Lemon{
    ConfigurationValue& ConfigurationParser::Find(const std::string& s){
        return root[s];
    }
}