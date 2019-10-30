/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_bmp.c
**
** Bitmap object manipulation routines
**
** $Id: $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tl_types.h"
#include "tl_bmp.h"


#define  BPP_PITCH(pitch, bpp)   ((pitch) * (((bpp) + 7) / 8))


/* TODO: you can make this faster. */
void thin_bmp_clear(const bitmap_t *bitmap, uint8 color)
{
   memset(bitmap->data, color, bitmap->pitch * bitmap->height);
}

static bitmap_t *_make_bitmap(uint8 *data_addr, bool hw, int width, 
                              int height, int bpp, int pitch, int overdraw)
{
   bitmap_t *bitmap;
   int i;

   /* sometimes our data address is zero; for instance, setting
   ** video selectors for VESA mode with far pointers.  so we
   ** don't want to bail out if we're passed a zero address.
   */

   /* Make sure to add in space for line pointers */
   bitmap = malloc(sizeof(bitmap_t) + (sizeof(uint8 *) * height));
   if (NULL == bitmap)
      return NULL;

   bitmap->hardware = hw;
   bitmap->height = height;
   bitmap->width = width;
   bitmap->bpp = bpp;
   bitmap->data = data_addr;
   bitmap->pitch = BPP_PITCH(pitch, bpp);

   /* Set up line pointers */
   bitmap->line[0] = bitmap->data + overdraw;

   for (i = 1; i < height; i++)
      bitmap->line[i] = bitmap->line[i - 1] + bitmap->pitch;

   return bitmap;
}

/* Allocate and initialize a bitmap structure */
bitmap_t *thin_bmp_create(int width, int height, int bpp, int overdraw)
{
   uint8 *addr;
   
   /* left and right overdraw */
   int pitch = width + (overdraw * 2);

   /* dword align */
   pitch = (pitch + 3) & ~3;

   addr = malloc(height * BPP_PITCH(pitch, bpp));
   if (NULL == addr)
      return NULL;

   return _make_bitmap(addr, false, width, height, bpp, pitch, overdraw);
}

/* allocate and initialize a hardware bitmap */
bitmap_t *thin_bmp_createhw(uint8 *addr, int width, int height, int bpp, int pitch)
{
   return _make_bitmap(addr, true, width, height, bpp, pitch, 0); /* zero overdraw */
}

/* Deallocate space for a bitmap structure */
void thin_bmp_destroy(bitmap_t **bitmap)
{
   if (*bitmap)
   {
      if ((*bitmap)->data && false == (*bitmap)->hardware)
         free((*bitmap)->data);
      free(*bitmap);
      *bitmap = NULL;
   }
}

/*
** $Log: $
*/
