#pragma once

#include "window.h"
#include "input.h"

enum Action {
    ActionNone,
    ActionCloseWindow,
    ActionGUI,
    ActionMinimize,
    ActionMaximize,
};

extern Window* active;