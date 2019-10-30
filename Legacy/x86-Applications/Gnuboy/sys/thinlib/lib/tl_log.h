/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_log.h
**
** Error logging header file
**
** $Id: $
*/

#ifndef _TL_LOG_H_
#define _TL_LOG_H_

extern void thin_printf(const char *format, ... );
extern void thin_setlogfunc(int (*logfunc)(const char *string, ... ));
extern void thin_assert(int expr, int line, const char *file, char *msg);

#endif /* !_TL_LOG_H_ */

/*
** $Log: $
*/
