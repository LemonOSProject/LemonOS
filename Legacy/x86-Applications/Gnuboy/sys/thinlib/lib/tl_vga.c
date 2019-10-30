/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_vga.c
**
** VGA-related functions
**
** $Id: $
*/

#include <stdio.h>
#include <string.h>
#include <dpmi.h>
#include <dos.h>
#include "tl_types.h"
#include "tl_bmp.h"
#include "tl_video.h"
#include "tl_vga.h"
#include "tl_djgpp.h"
#include "tl_int.h"

#define  DEFAULT_OVERSCAN        0

#define  MODE_TEXT               0x03
#define  MODE_13H                0x13

#define  VGA_ADDRESS             0xA0000  /* we love segments! */

/* VGA card register addresses */
#define  VGA_ATTR                0x3C0 /* Attribute reg */
#define  VGA_MISC                0x3C2 /* Misc. output register */
#define  VGA_SEQ_ADDR            0x3C4 /* Base port of sequencer */
#define  VGA_SEQ_DATA            0x3C5 /* Data port of sequencer */
#define  VGA_CRTC_ADDR           0x3D4 /* Base port of CRT controller */
#define  VGA_CRTC_DATA           0x3D5 /* Data port of CRT controller */
#define  VGA_STATUS              0x3DA /* Input status #1 register */

#define  VGA_PAL_READ            0x3C7 /* Palette read address */
#define  VGA_PAL_WRITE           0x3C8 /* Palette write address */
#define  VGA_PAL_DATA            0x3C9 /* Palette data register */

/* generic VGA CRTC register indexes */
#define  HZ_DISPLAY_TOTAL        0x00
#define  HZ_DISPLAY_END          0x01
#define  CRTC_OVERFLOW           0x07
#define  VT_DISPLAY_END          0x12
#define  MEM_OFFSET              0x13

/* indices into our register array */
#define  CLOCK_INDEX             0
#define  H_TOTAL_INDEX           1
#define  H_DISPLAY_INDEX         2
#define  H_BLANKING_START_INDEX  3
#define  H_BLANKING_END_INDEX    4
#define  H_RETRACE_START_INDEX   5
#define  H_RETRACE_END_INDEX     6
#define  V_TOTAL_INDEX           7
#define  OVERFLOW_INDEX          8
#define  MAXIMUM_SCANLINE_INDEX  10
#define  V_RETRACE_START_INDEX   11
#define  V_RETRACE_END_INDEX     12
#define  V_END_INDEX             13
#define  MEM_OFFSET_INDEX        14
#define  UNDERLINE_LOC_INDEX     15
#define  V_BLANKING_START_INDEX  16
#define  V_BLANKING_END_INDEX    17
#define  MODE_CONTROL_INDEX      18
#define  MEMORY_MODE_INDEX       20


typedef struct vgareg_s
{
   int port;
   int index;
   uint8 value;
} vgareg_t;

typedef struct vgamode_s
{
   int width;
   int height;
   char *name;
   vgareg_t *regs;
} vgamode_t;

/* 60 Hz */
static vgareg_t mode_256x224[] =
{
   { 0x3C2, 0x00, 0xE3 }, { 0x3D4, 0x00, 0x5F }, { 0x3D4, 0x01, 0x3F },
   { 0x3D4, 0x02, 0x40 }, { 0x3D4, 0x03, 0x82 }, { 0x3D4, 0x04, 0x4A },
   { 0x3D4, 0x05, 0x9A }, { 0x3D4, 0x06, 0x0B }, { 0x3D4, 0x07, 0xB2 },
   { 0x3D4, 0x08, 0x00 }, { 0x3D4, 0x09, 0x61 }, { 0x3d4, 0x10, 0x00 },
   { 0x3D4, 0x11, 0xAC }, { 0x3D4, 0x12, 0xBF }, { 0x3D4, 0x13, 0x20 },
   { 0x3D4, 0x14, 0x40 }, { 0x3D4, 0x15, 0x01 }, { 0x3D4, 0x16, 0x0A },
   { 0x3D4, 0x17, 0xA3 }, { 0x3C4, 0x01, 0x01 }, { 0x3C4, 0x04, 0x0E },
   { 0, 0, 0 }
};

static vgareg_t mode_256x240[] =
{
   { 0x3c2, 0x00, 0xe3},{ 0x3d4, 0x00, 0x55},{ 0x3d4, 0x01, 0x3f},
   { 0x3d4, 0x02, 0x80},{ 0x3d4, 0x03, 0x90},{ 0x3d4, 0x04, 0x49},
   { 0x3d4, 0x05, 0x80},{ 0x3D4, 0x06, 0x43},{ 0x3d4, 0x07, 0xb2},
   { 0x3d4, 0x08, 0x00},{ 0x3D4, 0x09, 0x61},{ 0x3d4, 0x10, 0x04},
   { 0x3d4, 0x11, 0xac},{ 0x3D4, 0x12, 0xdf},{ 0x3d4, 0x13, 0x20},
   { 0x3d4, 0x14, 0x40},{ 0x3d4, 0x15, 0x07},{ 0x3D4, 0x16, 0x11},
   { 0x3d4, 0x17, 0xa3},{ 0x3c4, 0x01, 0x01},{ 0x3c4, 0x04, 0x0e},
   { 0, 0, 0 }
};

static vgareg_t mode_256x256[] =
{
   { 0x3C2, 0x00, 0xE3 }, { 0x3D4, 0x00, 0x5F }, { 0x3D4, 0x01, 0x3F },
   { 0x3D4, 0x02, 0x40 }, { 0x3D4, 0x03, 0x82 }, { 0x3D4, 0x04, 0x4A },
   { 0x3D4, 0x05, 0x9A }, { 0x3D4, 0x06, 0x23 }, { 0x3D4, 0x07, 0xB2 },
   { 0x3D4, 0x08, 0x00 }, { 0x3D4, 0x09, 0x61 }, { 0x3D4, 0x10, 0x0A },
   { 0x3D4, 0x11, 0xAC }, { 0x3D4, 0x12, 0xFF }, { 0x3D4, 0x13, 0x20 },
   { 0x3D4, 0x14, 0x40 }, { 0x3D4, 0x15, 0x07 }, { 0x3D4, 0x16, 0x1A },
   { 0x3D4, 0x17, 0xA3 }, { 0x3C4, 0x01, 0x01 }, { 0x3C4, 0x04, 0x0E },
   { 0, 0, 0 }
};

/* 60 Hz */
static vgareg_t mode_256x256wide[] =
{
   { 0x3C2, 0x00, 0xE3 }, { 0x3D4, 0x00, 0x52 }, { 0x3D4, 0x01, 0x3F },
   { 0x3D4, 0x02, 0x80 }, { 0x3D4, 0x03, 0x90 }, { 0x3D4, 0x04, 0x49 },
   { 0x3D4, 0x05, 0x80 }, { 0x3D4, 0x06, 0x55 }, { 0x3D4, 0x07, 0xB2 },
   { 0x3D4, 0x08, 0x00 }, { 0x3D4, 0x09, 0x61 }, { 0x3D4, 0x10, 0x20 },
   { 0x3D4, 0x11, 0xAC }, { 0x3D4, 0x12, 0xFF }, { 0x3D4, 0x13, 0x20 },
   { 0x3D4, 0x14, 0x40 }, { 0x3D4, 0x15, 0x07 }, { 0x3D4, 0x16, 0x1A },
   { 0x3D4, 0x17, 0xA3 }, { 0x3C4, 0x01, 0x01 }, { 0x3C4, 0x04, 0x0E },
   { 0, 0, 0 }
};

/* 60 Hz */
static vgareg_t mode_288x224[] =
{
   { 0x3C2, 0x00, 0xE3 }, { 0x3D4, 0x00, 0x5F }, { 0x3D4, 0x01, 0x47 },
   { 0x3D4, 0x02, 0x50 }, { 0x3D4, 0x03, 0x82 }, { 0x3D4, 0x04, 0x50 },
   { 0x3D4, 0x05, 0x80 }, { 0x3D4, 0x06, 0x08 }, { 0x3D4, 0x07, 0x3E },
   { 0x3D4, 0x08, 0x00 }, { 0x3D4, 0x09, 0x41 }, { 0x3D4, 0x10, 0xDA },
   { 0x3D4, 0x11, 0x9C }, { 0x3D4, 0x12, 0xBF }, { 0x3D4, 0x13, 0x24 },
   { 0x3D4, 0x14, 0x40 }, { 0x3D4, 0x15, 0xC7 }, { 0x3D4, 0x16, 0x04 },
   { 0x3D4, 0x17, 0xA3 }, { 0x3C4, 0x01, 0x01 }, { 0x3C4, 0x04, 0x0E },
   { 0, 0, 0 }
};

static vgareg_t mode_320x200[] =
{
   { 0, 0, 0 }
};

static vgamode_t vidmodes[] =
{
   { 288, 224, "288x224", mode_288x224 },
   { 256, 224, "256x224", mode_256x224 },
   { 256, 240, "256x240", mode_256x240 },
   { 256, 256, "256x256 (wide)", mode_256x256wide },
   { 256, 256, "256x256", mode_256x256 },
   { 320, 200, "320x200", mode_320x200 },
   { 0, 0, NULL, 0 }
};


static bitmap_t *screen = NULL;
static bitmap_t *hardware = NULL;

/* current VGA mode */
static vgamode_t *vga_mode = NULL;
static bool vga_hardware = false;

/* Set a VGA mode */
static void vga_setvgamode(uint8 mode)
{
   __dpmi_regs r;
   r.x.ax = mode;
   __dpmi_int(0x10, &r);
}

static void vga_set_overscan(int index)
{
   outportb(VGA_ATTR, 0x31);
   outportb(VGA_ATTR, index);
}

static void vga_outregs(vgareg_t *reg)
{
   uint8 crtc_val;

   /* Disable interrupts, wait for vertical retrace */
//   thin_vga_waitvsync();
   THIN_DISABLE_INTS();

   /* Sequencer reset */
   outportb(VGA_SEQ_ADDR, 0x00);
   outportb(VGA_SEQ_DATA, 0x01);
   crtc_val = inportb(VGA_CRTC_DATA) & 0x7F;
   
   /* Unprotect registers 0-7 */
   outportb(VGA_CRTC_ADDR, 0x11);
   outportb(VGA_CRTC_DATA, crtc_val);

   /* Reset read/write flip-flop */
   inportb(VGA_STATUS);

   /* Do the icky register stuff */
   while (reg->port)
   {
      switch(reg->port)
      {
      case VGA_ATTR:
         /* Reset read/write flip-flop */
         inportb(VGA_STATUS);
         /* Ensure VGA output is enabled - bit 5 */
         outportb(VGA_ATTR, reg->index | 0x20);
         outportb(VGA_ATTR, reg->value);
         break;

      case VGA_MISC:
         /* Write directly to port */
         outportb(reg->port, reg->value);
         break;
      
      case VGA_SEQ_ADDR:
      case VGA_CRTC_ADDR:
      default:
         /* Index to port, value to port + 1 */
         outportb(reg->port, reg->index);
         outportb(reg->port + 1, reg->value);
         break;
      }

      reg++;
   }

   /* Set overscan color */
   vga_set_overscan(DEFAULT_OVERSCAN);

   /* Clear sequencer reset */
   outportb(VGA_SEQ_ADDR, 0x00);
   outportb(VGA_SEQ_DATA, 0x03);

   THIN_ENABLE_INTS();
}

/* Set up VGA mode 13h, then tweak it appropriately */
int thin_vga_setmode(int width, int height, int bpp)
{
   if (8 != bpp)
      return -1;

   vga_mode = vidmodes;

   /* Search for the video mode */
   while ((vga_mode->width != width) || (vga_mode->height != height))
   {
      if (NULL == vga_mode->regs)
      {
         vga_mode = NULL;
         return -1;
      }
      vga_mode++;
   }

   /* Set up our standard mode 13h */
   vga_setvgamode(MODE_13H);

   vga_outregs(vga_mode->regs);

   return 0;
}

/* Destroy VGA */
void thin_vga_shutdown(void)
{
   /* set textmode */
   vga_setvgamode(MODE_TEXT);

   if (screen)
      thin_bmp_destroy(&screen);

   if (hardware)
      thin_bmp_destroy(&hardware);
}

/* Initialize VGA */
int thin_vga_init(int width, int height, int bpp, int param)
{
   if (8 != bpp)
      return -1;

   /* ensure we really want a hardware surface */
   if (thinlib_nearptr && (param & THIN_VIDEO_HWSURFACE))
   {
      vga_hardware = true;
      screen = thin_bmp_createhw((uint8 *) THIN_PHYSICAL_ADDR(VGA_ADDRESS),
                                 width, height, bpp, width);
      if (NULL == screen)
         return -1;
   }
   else
   {
      vga_hardware = false;
      hardware = thin_bmp_createhw((uint8 *) VGA_ADDRESS,
                                   width, height, bpp, width);
      if (NULL == hardware)
         return -1;

      screen = thin_bmp_create(width, height, bpp, 0);
      if (NULL == screen)
         return -1;
   }

   /* Set the initial video mode, no scanlines */
   if (thin_vga_setmode(width, height, bpp))
   {
      thin_vga_shutdown();
      return -1;
   }

   return 0;
}

/* cram an 8-bit, 256 entry rgb palette into 6-bit vga */
void thin_vga_setpalette(rgb_t *palette, int index, int length)
{
   int i;
   
   /* we also want to find the closest color index to black,
   ** and set that as our overscan color
   */
   int overscan_index = 0;
   int overscan_sum = 255 * 3;

   outportb(VGA_PAL_WRITE, index);

   for (i = 0; i < length; i++)
   {
      if (palette[i].r + palette[i].g + palette[i].b < overscan_sum)
      {
         overscan_sum = palette[i].r + palette[i].g + palette[i].b;
         overscan_index = index + i;
      }

      outportb(VGA_PAL_DATA, palette[i].r >> 2);
      outportb(VGA_PAL_DATA, palette[i].g >> 2);
      outportb(VGA_PAL_DATA, palette[i].b >> 2);
   }

   vga_set_overscan(overscan_index);
}

void thin_vga_waitvsync(void)
{
   while (0 == (inportb(VGA_STATUS) & 0x08));
   //while (inportb(VGA_STATUS) & 0x08);
}

bitmap_t *thin_vga_lockwrite(void)
{
   /* always return screen */
   return screen;
}

void thin_vga_freewrite(int num_dirties, rect_t *dirty_rects)
{
   UNUSED(num_dirties);
   UNUSED(dirty_rects);

   if (false == vga_hardware)
   {
      dosmemput(screen->line[0], hardware->pitch * hardware->height,
                (int) hardware->line[0]);
   }
}

/*
** $Log: $
*/
