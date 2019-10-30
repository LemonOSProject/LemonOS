/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_types.h
**
** type definitions for thinlib
**
** $Id: $
*/

#ifndef _TL_TYPES_
#define _TL_TYPES_

/* TODO: rethink putting these here. */
#ifdef THINLIB_DEBUG

#include "tl_log.h"

#define  THIN_ASSERT(expr)    thin_assert((int) (expr), __LINE__, __FILE__, NULL)
#define  THIN_ASSERT_MSG(msg) thin_assert(false, __LINE__, __FILE__, (msg))

#else /* !THINLIB_DEBUG */

#define  THIN_ASSERT(expr)
#define  THIN_ASSERT_MSG(msg)

#endif /* !THINLIB_DEBUG */

/* quell stupid compiler warnings */
#define  UNUSED(x)   ((x) = (x))

typedef  signed char    int8;
typedef  signed short   int16;
typedef  signed int     int32;
typedef  unsigned char  uint8;
typedef  unsigned short uint16;
typedef  unsigned int   uint32;

#ifndef __cplusplus
#undef   false
#undef   true
#undef   NULL

typedef enum
{
   false = 0,
   true = 1
} bool;

#ifndef  NULL
#define  NULL     ((void *) 0)
#endif

#endif /* !__cplusplus */

#endif /* !_TL_TYPES_ */

/*
** $Log: $
*/
