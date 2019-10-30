/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_sb.h
**
** DOS Sound Blaster header file
**
** $Id: $
*/

#ifndef _TL_SB_H_
#define _TL_SB_H_

typedef void (*sbmix_t)(void *user_data, void *buffer, int size);

/* Sample format bitfields */
#define  SB_FORMAT_8BIT       0x00
#define  SB_FORMAT_16BIT      0x01
#define  SB_FORMAT_MONO       0x00
#define  SB_FORMAT_STEREO     0x02
#define  SB_FORMAT_UNSIGNED   0x00
#define  SB_FORMAT_SIGNED     0x04

extern int  thin_sb_init(int *sample_rate, int *buf_size, int *format);
extern void thin_sb_shutdown(void);
extern int  thin_sb_start(sbmix_t fillbuf, void *user_data);
extern void thin_sb_stop(void);
extern void thin_sb_setrate(int rate);

#endif /* !_TL_SB_H_ */

/*
** $Log: $
*/
