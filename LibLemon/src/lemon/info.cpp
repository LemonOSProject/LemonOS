#include <lemon/info.h>
#include <lemon/syscall.h>

namespace Lemon{
    lemon_sysinfo_t SysInfo(){
        lemon_sysinfo_t info;
        syscall(SYS_INFO, &info, 0, 0, 0, 0);
        return info;
    }
}