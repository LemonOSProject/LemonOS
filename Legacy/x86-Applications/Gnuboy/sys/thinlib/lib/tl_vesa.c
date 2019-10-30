/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_vesa.c
**
** VESA code.
**
** $Id: $
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <go32.h>
#include <dos.h>
#include <dpmi.h>

#include "tl_types.h"
#include "tl_log.h"
#include "tl_bmp.h"

#include "tl_djgpp.h"

#include "tl_video.h"
#include "tl_vesa.h"

#define  __PACKED__  __attribute__ ((packed))

/* VESA information block structure */
typedef struct vesainfo_s
{
   char   VESASignature[4]    __PACKED__;
   uint16 VESAVersion         __PACKED__;
   uint32 OEMStringPtr        __PACKED__;
   char   Capabilities[4]     __PACKED__;
   uint32 VideoModePtr        __PACKED__; 
   uint16 TotalMemory         __PACKED__; 
   uint16 OemSoftwareRev      __PACKED__; 
   uint32 OemVendorNamePtr    __PACKED__; 
   uint32 OemProductNamePtr   __PACKED__; 
   uint32 OemProductRevPtr    __PACKED__; 
   uint8  Reserved[222]       __PACKED__; 
} vesainfo_t;

/* SuperVGA mode information block */
typedef struct modeinfo_s
{
   uint16 ModeAttributes      __PACKED__; 
   uint8  WinAAttributes      __PACKED__; 
   uint8  WinBAttributes      __PACKED__; 
   uint16 WinGranularity      __PACKED__; 
   uint16 WinSize             __PACKED__; 
   uint16 WinASegment         __PACKED__; 
   uint16 WinBSegment         __PACKED__; 
   uint32 WinFuncPtr          __PACKED__; 
   uint16 BytesPerScanLine    __PACKED__; 
   uint16 XResolution         __PACKED__; 
   uint16 YResolution         __PACKED__; 
   uint8  XCharSize           __PACKED__; 
   uint8  YCharSize           __PACKED__; 
   uint8  NumberOfPlanes      __PACKED__; 
   uint8  BitsPerPixel        __PACKED__; 
   uint8  NumberOfBanks       __PACKED__; 
   uint8  MemoryModel         __PACKED__; 
   uint8  BankSize            __PACKED__; 
   uint8  NumberOfImagePages  __PACKED__;
   uint8  Reserved_page       __PACKED__; 
   uint8  RedMaskSize         __PACKED__; 
   uint8  RedMaskPos          __PACKED__; 
   uint8  GreenMaskSize       __PACKED__; 
   uint8  GreenMaskPos        __PACKED__;
   uint8  BlueMaskSize        __PACKED__; 
   uint8  BlueMaskPos         __PACKED__; 
   uint8  ReservedMaskSize    __PACKED__; 
   uint8  ReservedMaskPos     __PACKED__; 
   uint8  DirectColorModeInfo __PACKED__;

   /* VBE 2.0 extensions */
   uint32 PhysBasePtr         __PACKED__; 
   uint32 OffScreenMemOffset  __PACKED__; 
   uint16 OffScreenMemSize    __PACKED__; 

   /* VBE 3.0 extensions */
   uint16 LinBytesPerScanLine __PACKED__;
   uint8  BnkNumberOfPages    __PACKED__;
   uint8  LinNumberOfPages    __PACKED__;
   uint8  LinRedMaskSize      __PACKED__;
   uint8  LinRedFieldPos      __PACKED__;
   uint8  LinGreenMaskSize    __PACKED__;
   uint8  LinGreenFieldPos    __PACKED__;
   uint8  LinBlueMaskSize     __PACKED__;
   uint8  LinBlueFieldPos     __PACKED__;
   uint8  LinRsvdMaskSize     __PACKED__;
   uint8  LinRsvdFieldPos     __PACKED__;
   uint32 MaxPixelClock       __PACKED__;

   uint8  Reserved[190]       __PACKED__; 
} modeinfo_t;


#define  MASK_LINEAR(addr)    (addr & 0x000FFFFF)
#define  RM_TO_LINEAR(addr)   (((addr & 0xFFFF0000) >> 12) + (addr & 0xFFFF))
#define  RM_OFFSET(addr)      (addr & 0xF)
#define  RM_SEGMENT(addr)     ((addr >> 4) & 0xFFFF)

#define  VBE_LINEAR_ADDR      0x4000
#define  VBE_LINEAR_AVAIL     0x0080

#define  VBE_INT              0x10
#define  VBE_SUCCESS          0x004F
#define  VBE_FUNC_DETECT      0x4F00
#define  VBE_FUNC_GETMODEINFO 0x4F01
#define  VBE_FUNC_SETMODE     0x4F02
#define  VBE_FUNC_GETMODE     0x4F03
#define  VBE_FUNC_FLIPPAGE    0x4F07

#define  MAX_NUM_MODES        256

short int vid_selector = -1;
static uint16 modelist[MAX_NUM_MODES];
static bitmap_t *screen = NULL;
static bitmap_t *hardware = NULL;
static int total_memory = 0;
static bool vesa_hardware = false;

/* look for vesa */
static int vesa_detect(void)
{
   vesainfo_t vesa_info;
   __dpmi_regs regs;
   long list_ptr;
   int mode_pos;

   /* Use DOS transfer buffer to hold VBE info */
   THIN_ASSERT(sizeof(vesainfo_t) < _go32_info_block.size_of_transfer_buffer);
   memset(&regs, 0, sizeof(__dpmi_regs));

   strncpy(vesa_info.VESASignature, "VBE2", 4);
   dosmemput(&vesa_info, sizeof(vesainfo_t), MASK_LINEAR(__tb));
   
   regs.x.ax = VBE_FUNC_DETECT;
   regs.x.es = RM_SEGMENT(__tb);
   regs.x.di = RM_OFFSET(__tb);

   __dpmi_int(VBE_INT, &regs);
   if (VBE_SUCCESS != regs.x.ax)
      return -1;

   dosmemget(MASK_LINEAR(__tb), sizeof(vesainfo_t), &vesa_info);
   if (strncmp(vesa_info.VESASignature, "VESA", 4) != 0)
      return -1;

   /* check to see if linear framebuffer is available */
   if ((vesa_info.VESAVersion >> 8) < 2)
   {
      thin_printf("thinlib.vesa: no linear framebuffer available\n");
      return -1;
   }

   /* build list of available modes */
   memset(&modelist, 0, MAX_NUM_MODES * sizeof(uint16));
   mode_pos = 0;
   
   list_ptr = RM_TO_LINEAR(vesa_info.VideoModePtr);
   while (1)
   {
      uint16 mode;

      dosmemget(list_ptr + mode_pos * 2, 2, &mode);

      if (0xFFFF == mode)
      {
         modelist[mode_pos] = 0;
         break;
      }

      modelist[mode_pos++] = mode;
   }

   total_memory = vesa_info.TotalMemory;
  
   return 0;
}

static int vesa_getmodeinfo(uint16 mode, modeinfo_t *modeinfo)
{
   __dpmi_regs regs;

   THIN_ASSERT(sizeof(modeinfo_t) < _go32_info_block.size_of_transfer_buffer);

   memset(&regs, 0, sizeof(regs));
   regs.x.ax = VBE_FUNC_GETMODEINFO; 
   regs.x.cx = mode;
   regs.x.es = RM_SEGMENT(__tb);
   regs.x.di = RM_OFFSET(__tb);
  
   __dpmi_int(VBE_INT, &regs);
   if (VBE_SUCCESS != regs.x.ax)
      return -1;

   dosmemget(MASK_LINEAR(__tb), sizeof(modeinfo_t), modeinfo);
   return 0;
}

static uint16 vesa_findmode(int width, int height, int bpp)
{
   modeinfo_t mode_info;
   uint16 mode;
   int mode_pos;

   for (mode_pos = 0; ; mode_pos++)
   {
      mode = modelist[mode_pos];
      
      if (0 == mode)
         break;

      if (vesa_getmodeinfo(mode, &mode_info))
         break; /* we are definitely screwed */
      
      if (mode_info.XResolution == width && mode_info.YResolution == height 
          && mode_info.BitsPerPixel == bpp)
      {
         return mode;
      }
   }

   return 0;
}

int thin_vesa_setmode(int width, int height, int bpp)
{
   uint16 mode;
   __dpmi_regs regs;
   __dpmi_meminfo mi;
   modeinfo_t mode_info;
   unsigned int address;

   mode = vesa_findmode(width, height, bpp);
   if (0 == mode)
   {
      thin_printf("thinlib.vesa: yikes, couldn't find mode\n");
      return -1;
   }

   if (vesa_getmodeinfo(mode, &mode_info))
   {
      thin_printf("thinlib.vesa: error in vesa_getmodeinfo\n");
      return -1;
   }

   mi.size = mode_info.BytesPerScanLine * mode_info.YResolution;
   mi.address = mode_info.PhysBasePtr;
   if (-1 == __dpmi_physical_address_mapping(&mi))
   {
      thin_printf("thinlib.vesa: error in __dpmi_physical_address_mapping\n");
      return -1;
   }

   if (false == vesa_hardware)
   {
      vid_selector = __dpmi_allocate_ldt_descriptors(1);
      if (-1 == vid_selector)
      {
         thin_printf("thinlib.vesa: error in __dpmi_allocate_ldt_descriptors\n");
         return -1;
      }

      /* paranoid */
      if (-1 == __dpmi_set_descriptor_access_rights(vid_selector, 0x40f3))
      {
         thin_printf("thinlib.vesa: error in __dpmi_set_descriptor_access_rights\n");
         return -1;
      }

      if (-1 == __dpmi_set_segment_base_address(vid_selector, mi.address))
      {
         thin_printf("thinlib.vesa: error in __dpmi_set_segment_base_address\n");
         return -1;
      }

      if (-1 == __dpmi_set_segment_limit(vid_selector, total_memory << 16 | 0xfff))
      {
         thin_printf("thinlib.vesa: error in __dpmi_set_segment_limit\n");
         return -1;
      }
   }

   memset(&regs, 0, sizeof(regs));
   regs.x.ax = VBE_FUNC_SETMODE;
   regs.x.bx = mode | VBE_LINEAR_ADDR;
 
   __dpmi_int(VBE_INT, &regs);
   if (VBE_SUCCESS != regs.x.ax)
   {
      thin_printf("thinlib.vesa: vesa dpmi int failed\n");
      return -1;
   }

   if (false == vesa_hardware)
      address = 0;
   else
      address = mi.address;

   if (NULL != screen)
      thin_bmp_destroy(&screen);

   if (vesa_hardware)
   {
      screen = thin_bmp_createhw((uint8 *) THIN_PHYSICAL_ADDR(address),
                                 mode_info.XResolution, mode_info.YResolution, 
                                 mode_info.BitsPerPixel, mode_info.XResolution);
      if (NULL == screen)
      {
         thin_printf("thinlib.vesa: failed creating hardware surface\n");
         return -1;
      }
   }
   else
   {
      if (NULL != hardware)
         thin_bmp_destroy(&hardware);

      hardware = thin_bmp_createhw((uint8 *) address, 
                                   mode_info.XResolution, mode_info.YResolution, 
                                   mode_info.BitsPerPixel, mode_info.XResolution);
      if (NULL == hardware)
      {
         thin_printf("thinlib.vesa: failed creating hardware surface\n");
         return -1;
      }

      screen = thin_bmp_create(mode_info.XResolution, mode_info.YResolution, 
                               mode_info.BitsPerPixel, 0);
      if (NULL == screen)
      {
         thin_printf("thinlib.vesa: failed creating software surface\n");
         return -1;
      }
   }

   return 0;
}

void thin_vesa_shutdown(void)
{
   __dpmi_regs regs;

   /* set text mode */
   memset(&regs, 0, sizeof(regs));
   regs.x.ax = 0x0003;
   __dpmi_int(VBE_INT, &regs);

   if (NULL != screen)
      thin_bmp_destroy(&screen);

   if (NULL != hardware)
      thin_bmp_destroy(&hardware);
}

int thin_vesa_init(int width, int height, int bpp, int param)
{
   screen = NULL;
   hardware = NULL;
   
   /* check to see if VESA card is present */
   if (vesa_detect())
      return -1;

   if (thinlib_nearptr && (param & THIN_VIDEO_HWSURFACE))
      vesa_hardware = true;
   else
      vesa_hardware = false;

   return thin_vesa_setmode(width, height, bpp);
}

bitmap_t *thin_vesa_lockwrite(void)
{
   return screen;
}


void thin_vesa_freewrite(int num_dirties, rect_t *dirty_rects)
{
   UNUSED(num_dirties);
   UNUSED(dirty_rects);

   /* if we don't have a hardware surface, blat it out */
   if (false == vesa_hardware)
   {
      _movedatal(_my_ds(), (unsigned) screen->line[0],
               vid_selector, (unsigned) hardware->line[0],
               hardware->pitch * hardware->height / 4);
   }
}

/*
** $Log: $
*/
