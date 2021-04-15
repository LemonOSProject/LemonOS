#pragma once 

#include <stdint.h>
#include <Lemon/System/ABI/Framebuffer.h>

#ifndef __lemon__
    #error "Lemon OS Only"
#endif

long LemonMapFramebuffer(void** ptr, FBInfo& fbInfo);