#pragma once

#include <Lemon/System/ABI/Framebuffer.h>
#include <stdint.h>

#ifndef __lemon__
#error "Lemon OS Only"
#endif

long LemonMapFramebuffer(void** ptr, FBInfo& fbInfo);
