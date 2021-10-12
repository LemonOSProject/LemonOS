#pragma once

#include <string>

using TestFunction = int(*)();

struct Test {
    TestFunction func;
    std::string prettyName;
};
