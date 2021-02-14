#pragma once 

#include <stdint.h>
#include <lemon/system/abi/fb.h>

#ifndef __lemon__
    #error "Lemon OS Only"
#endif

volatile uint8_t* LemonMapFramebuffer(FBInfo& fbInfo);