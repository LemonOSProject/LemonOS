#pragma once

#include <map>
#include <stdio.h>
#include <string>
#include <vector>

class CFGParser {
    struct CFGItem {
        std::string name;
        std::string value;
    };

  private:
    std::vector<std::pair<std::string, std::vector<CFGItem>>> items;
    FILE* cfgFile = nullptr;
    std::vector<char> cfgData;

  public:
    CFGParser(const char* path);
    ~CFGParser();

    void Parse();
    auto& GetItems() { return items; };
};
