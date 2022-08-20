#include <lemon/system/Info.h>
#include <lemon/syscall.h>

namespace Lemon {
lemon_sysinfo_t SysInfo() {
    lemon_sysinfo_t info;
    syscall(SYS_INFO, &info);
    return info;
}
} // namespace Lemon
