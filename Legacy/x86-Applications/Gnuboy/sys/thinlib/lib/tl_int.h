/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_int.h
**
** interrupt handling stuff
**
** $Id: $
*/

#ifndef _TL_INT_H_
#define _TL_INT_H_

#define  THIN_DISABLE_INTS()        __asm__ __volatile__ ("cli")
#define  THIN_ENABLE_INTS()         __asm__ __volatile__ ("sti")

typedef int (*inthandler_t)(void);

extern int thin_int_install(int chan, inthandler_t handler);
extern void thin_int_remove(int chan);

extern void thin_irq_restore(int irq);
extern void thin_irq_enable(int irq);
extern void thin_irq_disable(int irq);

#endif /* !_TL_INT_H_ */

/*
** $Log: $
*/
