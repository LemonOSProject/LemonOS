/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_vga.h
**
** VGA-specific thinlib routines
**
** $Id: $
*/

#ifndef _TL_VGA_H_
#define _TL_VGA_H_

#include "tl_types.h"
#include "tl_bmp.h"

extern int thin_vga_init(int width, int height, int bpp, int param);
extern void thin_vga_shutdown(void);

extern int thin_vga_setmode(int width, int height, int bpp);
extern void thin_vga_setpalette(rgb_t *palette, int index, int length);
extern void thin_vga_waitvsync(void);

extern bitmap_t *thin_vga_lockwrite(void);
extern void thin_vga_freewrite(int num_dirties, rect_t *dirty_rects);

#endif /* !_TL_VGA_H_ */

/*
** $Log: $
*/
