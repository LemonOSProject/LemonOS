#pragma once

#include <objects/kobject.h>
#include <refptr.h>

struct Handle{
    unsigned id = 0;
    FancyRefPtr<KernelObject> ko;
};