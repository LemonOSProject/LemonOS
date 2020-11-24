#pragma once

#include <objects/kobject.h>
#include <refptr.h>

typedef long long handle_id_t;

struct Handle{
    handle_id_t id = 0;
    FancyRefPtr<KernelObject> ko;
};