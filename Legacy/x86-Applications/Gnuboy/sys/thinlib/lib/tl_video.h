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

#ifndef _TL_VIDEO_H_
#define _TL_VIDEO_H_

#include "tl_types.h"
#include "tl_bmp.h"

/* video driver capabilities */
#define  THIN_VIDEO_CUSTOMBLIT   0x0001
#define  THIN_VIDEO_SCANLINES    0x0002
#define  THIN_VIDEO_HWSURFACE    0x0004

extern int thin_vid_getcaps(void);

extern int thin_vid_init(int width, int height, int bpp, int param);
extern void thin_vid_shutdown(void);

extern int thin_vid_setmode(int width, int height, int bpp);
extern void thin_vid_setpalette(rgb_t *palette, int index, int length);

extern bitmap_t *thin_vid_lockwrite(void);
extern void thin_vid_freewrite(int num_dirties, rect_t *dirty_rects);

extern void thin_vid_customblit(bitmap_t *primary, int num_dirties,
                                rect_t *dirty_rects);

#endif /* !_TL_VIDEO_H_ */

/*
** $Log: $
*/
