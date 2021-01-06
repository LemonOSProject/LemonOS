#include <lemon/system/waitable.h>

#include <lemon/system/kobject.h>

namespace Lemon{
    void Waitable::Wait(){
        WaitForKernelObject(GetHandle());
    }
}