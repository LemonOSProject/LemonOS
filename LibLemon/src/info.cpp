#include <lemon/info.h>
#include <lemon/syscall.h>

lemon_sysinfo_t lemon_sysinfo(){
    lemon_sysinfo_t info;
    syscall(SYS_INFO, &info, 0, 0, 0, 0);
    return info;
}