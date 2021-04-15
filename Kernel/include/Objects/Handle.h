#pragma once

#include <Objects/KObject.h>
#include <RefPtr.h>
#include <Compiler.h>

typedef long long handle_id_t;

struct Handle {
    handle_id_t id = 0;
    FancyRefPtr<KernelObject> ko;
};