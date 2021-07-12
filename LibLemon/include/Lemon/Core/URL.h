#pragma once

#include <string>

namespace Lemon {
class URL final {
  public:
    URL(const char* url);

    inline const std::string& Protocol() const { return protocol; }
    inline const std::string& UserInfo() const { return userinfo; }
    inline const std::string& Host() const { return host; }
    inline const std::string& Port() const { return port; }
    inline const std::string& Resource() const { return resource; }

    inline bool IsValid() const { return valid; }

  private:
    std::string protocol;
    std::string userinfo;
    std::string host;
    std::string port;
    std::string resource;

    bool valid = false;
};
} // namespace Lemon
