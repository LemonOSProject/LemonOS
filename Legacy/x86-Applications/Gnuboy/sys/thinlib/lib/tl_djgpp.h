/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_djgpp.h
**
** djgpp frigs
**
** $Id: $
*/

#ifndef _TL_DJGPP_H_
#define _TL_DJGPP_H_

#include <dpmi.h>

/* interface to lock code and data */
#define  THIN_LOCKED_FUNC(x)        void x##_end(void) { }
#define  THIN_LOCKED_STATIC_FUNC(x) static void x##_end(void) { }
#define  THIN_LOCK_DATA(d, s)       _go32_dpmi_lock_data(d, s)
#define  THIN_LOCK_CODE(c, s)       _go32_dpmi_lock_code(c, s)
#define  THIN_LOCK_VAR(x)           THIN_LOCK_DATA((void *) &x, sizeof(x))
#define  THIN_LOCK_FUNC(x)          THIN_LOCK_CODE((void *) x, (long) x##_end - (long) x)

#include <sys/nearptr.h>
#define  THIN_PHYSICAL_ADDR(x)      ((x) + __djgpp_conventional_base)

extern int thinlib_nearptr;

#endif /* !_TL_DJGPP_H_ */

/*
** $Log: $
*/
