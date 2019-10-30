/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_log.c
**
** Error logging functions
**
** $Id: $
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "tl_types.h"
#include "tl_log.h"

#define  MAX_LOG_BUF_SIZE  1024

static int (*log_func)(const char *format, ... ) = printf;

void thin_printf(const char *format, ... )
{
   /* don't allocate on stack every call */
   static char buffer[MAX_LOG_BUF_SIZE + 1];
   va_list arg;

   va_start(arg, format);

   if (NULL != log_func)
   {
      vsprintf(buffer, format, arg);
      log_func(buffer);
   }

   va_end(arg);
}

void thin_setlogfunc(int (*func)(const char *format, ... ))
{
   log_func = func;
}

void thin_assert(int expr, int line, const char *file, char *msg)
{
   if (expr)
      return;

   if (NULL != msg)
      thin_printf("THIN_ASSERT: line %d of %s, %s\n", line, file, msg);
   else
      thin_printf("THIN_ASSERT: line %d of %s\n", line, file);

   exit(-1);
}

/*
** $Log: $
*/
