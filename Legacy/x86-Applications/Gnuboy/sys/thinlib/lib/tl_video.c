/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_video.h
**
** thinlib video routines
**
** $Id: $
*/

#include "tl_types.h"
#include "tl_video.h"
#include "tl_vesa.h"
#include "tl_vga.h"
#include "tl_log.h"

typedef struct viddriver_s
{
   const char *name;
   int      (*init)(int width, int height, int bpp, int param);
   void     (*shutdown)(void);
   int      (*setmode)(int width, int height, int bpp);
   void     (*setpalette)(rgb_t *palette, int index, int length);
   void     (*waitvsync)(void);
   bitmap_t *(*lock)(void);
   void     (*free)(int num_dirties, rect_t *dirty_rects);
   void     (*blit)(bitmap_t *primary, int num_dirties, rect_t *dirty_rects);
   int  caps;
} viddriver_t;

static viddriver_t vesa =
{
   "VESA 3.0 LFB",
   thin_vesa_init,
   thin_vesa_shutdown,
   thin_vesa_setmode,
   thin_vga_setpalette,
   thin_vga_waitvsync,
   thin_vesa_lockwrite,
   thin_vesa_freewrite,
   NULL,
   0,
};

static viddriver_t vga =
{
   "VGA",
   thin_vga_init,
   thin_vga_shutdown,
   thin_vga_setmode,
   thin_vga_setpalette,
   thin_vga_waitvsync,
   thin_vga_lockwrite,
   thin_vga_freewrite,
   NULL,
   0, /*THIN_VIDEO_SCANLINES,*/
};

static viddriver_t *driver_list[] = 
{
   &vesa,
   &vga,
   NULL
};

static viddriver_t driver;

int thin_vid_init(int width, int height, int bpp, int param)
{
   /* cascade driver checks by iterating through all drivers */
   viddriver_t **iter;

   for (iter = driver_list; *iter != NULL; iter++)
   {
      if (0 == (*iter)->init(width, height, bpp, param))
      {
         driver = **iter;
         return 0;
      }
   }

   driver.name = NULL;
   thin_printf("thinlib.video: could not find any matching video modes.\n");
   return -1;
}

void thin_vid_shutdown(void)
{
   if (NULL != driver.name)
   {
      driver.shutdown();
      memset(&driver, 0, sizeof(viddriver_t));
   }
}

int thin_vid_getcaps(void)
{
   return driver.caps;
}

int thin_vid_setmode(int width, int height, int bpp)
{
   if (driver.setmode(width, height, bpp))
   {
      thin_printf("thinlib.video: could not set %s video mode %dx%d %dbpp\n",
                  driver.name, width, height, bpp);
      return -1;
   }

   return 0;
}

void thin_vid_setpalette(rgb_t *palette, int index, int length)
{
   driver.setpalette(palette, index, length);
}

bitmap_t *thin_vid_lockwrite(void)
{
   return driver.lock();
}

void thin_vid_freewrite(int num_dirties, rect_t *dirty_rects)
{
   if (NULL != driver.free)
      driver.free(num_dirties, dirty_rects);
}

void thin_vid_customblit(bitmap_t *primary, int num_dirties,
                         rect_t *dirty_rects)
{
   THIN_ASSERT(driver.blit);

   driver.blit(primary, num_dirties, dirty_rects);
}

/*
** $Log: $
*/
