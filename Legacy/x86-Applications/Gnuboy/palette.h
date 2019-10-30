#ifndef PALETTE_H
#define PALETTE_H

#include "defs.h"

void pal_lock(byte n);
void pal_release(byte n);
void pal_expire();
void pal_set332();
byte pal_getcolor(int c, int r, int g, int b);

#endif

