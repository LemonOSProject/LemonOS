/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_bmp.h
**
** Bitmap object defines / prototypes
**
** $Id: $
*/

#ifndef _TL_BMP_H_
#define _TL_BMP_H_

#include "tl_types.h"

/* a bitmap rectangle */
typedef struct rect_s
{
   int16 x, y;
   uint16 w, h;
} rect_t;

typedef struct rgb_s
{
   int r, g, b;
} rgb_t;

typedef struct bitmap_s
{
   int width, height, pitch;
   int bpp;
   bool hardware;             /* is data a hardware region? */
   uint8 *data;               /* protected */
   uint8 *line[0];            /* will hold line pointers */
} bitmap_t;

extern void thin_bmp_clear(const bitmap_t *bitmap, uint8 color);
extern bitmap_t *thin_bmp_create(int width, int height, int bpp, int overdraw);
extern bitmap_t *thin_bmp_createhw(uint8 *addr, int width, int height, int bpp, int pitch);
extern void thin_bmp_destroy(bitmap_t **bitmap);

#endif /* !_TL_BMP_H_ */

/*
** $Log: $
*/
