#pragma once

#ifndef __lemon__
#error "Lemon OS Only"
#endif

#include <stdint.h>

typedef struct {
    uint64_t totalMem;
    uint64_t usedMem;
    uint16_t cpuCount;
} lemon_sysinfo_t;

namespace Lemon {
/////////////////////////////
/// \brief Get information about the system
///
/// Fill a lemon_sysinfo struct with information about memory and processor(s).
///
/// \return lemon_sysinfo_t
/////////////////////////////
lemon_sysinfo_t SysInfo();
} // namespace Lemon
