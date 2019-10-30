/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_dpp.h
**
** DOS DirectPad Pro scanning code prototypes
**
** $Id: $
*/

#ifndef _TL_DPP_H_
#define _TL_DPP_H_

#include "tl_types.h"

typedef struct dpp_s
{
/* private: */
   uint16 port; /* LPT port */
/* public: */
   int down, up, left, right;
   int b, a, select, start;
} dpp_t;

extern int thin_dpp_add(uint16 port, int pad_num);
extern int thin_dpp_init(void);
extern void thin_dpp_shutdown(void);
extern void thin_dpp_read(dpp_t *pad, int pad_num);

#endif /* !_TL_DPP_H_ */

/*
** $Log: $
*/
