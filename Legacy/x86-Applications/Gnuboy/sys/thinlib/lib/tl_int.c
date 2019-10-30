/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_int.c
**
** thinlib interrupt handling.  Thanks to Shawn Hargreaves
** and the Allegro project for providing adequate interrupt
** documentation.
**
** $Id: $
*/

#include <dos.h>
#include <go32.h>
#include <dpmi.h>
#include "thinlib.h"
#include "tl_types.h"
#include "tl_log.h"
#include "tl_djgpp.h"
#include "tl_int.h"

#define  PIC1_PORT   0x21
#define  PIC2_PORT   0xA1

/* Interrupt stuff */
typedef struct intr_s
{
   _go32_dpmi_seginfo old_interrupt;
   _go32_dpmi_seginfo new_interrupt;
   uint8 irq_vector;
   inthandler_t handler;
} intr_t;

#define  MAX_INTR    8

static bool pic_modified = false;
static uint8 pic1_mask, pic2_mask;
static uint8 pic1_orig, pic2_orig;

static intr_t intr[MAX_INTR];
static bool thin_int_locked = false;


#define  MAKE_INT_HANDLER(num)   \
static void _int_handler_##num##(void) \
{ \
   /* chain if necessary */ \
   if (intr[(num)].handler()) \
   { \
      void (*func)() = (void *) intr[(num)].old_interrupt.pm_offset; \
      func(); \
   } \
} \
THIN_LOCKED_STATIC_FUNC(_int_handler_##num##);

MAKE_INT_HANDLER(0)
MAKE_INT_HANDLER(1)
MAKE_INT_HANDLER(2)
MAKE_INT_HANDLER(3)
MAKE_INT_HANDLER(4)
MAKE_INT_HANDLER(5)
MAKE_INT_HANDLER(6)
MAKE_INT_HANDLER(7)

int thin_int_install(int chan, inthandler_t handler)
{
   int i;

   if (chan < 0)// || chan >= MAX_INTR)
      return -1;

   if (NULL == handler)
      return -1;

   if (false == thin_int_locked)
   {
      int i;

      for (i = 0; i < MAX_INTR; i++)
      {
         intr[i].handler = NULL;
         intr[i].irq_vector = 0;
      }

      THIN_LOCK_VAR(intr);
      THIN_LOCK_FUNC(_int_handler_0);
      THIN_LOCK_FUNC(_int_handler_1);
      THIN_LOCK_FUNC(_int_handler_2);
      THIN_LOCK_FUNC(_int_handler_3);
      THIN_LOCK_FUNC(_int_handler_4);
      THIN_LOCK_FUNC(_int_handler_5);
      THIN_LOCK_FUNC(_int_handler_6);
      THIN_LOCK_FUNC(_int_handler_7);

      thin_int_locked = true;
   }

   /* find a free slot */
   for (i = 0; i < MAX_INTR; i++)
   {
      if (NULL == intr[i].handler)
      {
         intr_t *pintr = &intr[i];

         pintr->new_interrupt.pm_selector = _go32_my_cs();
 
         switch (i)
         {
         case 0:  pintr->new_interrupt.pm_offset = (int) _int_handler_0;   break;
         case 1:  pintr->new_interrupt.pm_offset = (int) _int_handler_1;   break;
         case 2:  pintr->new_interrupt.pm_offset = (int) _int_handler_2;   break;
         case 3:  pintr->new_interrupt.pm_offset = (int) _int_handler_3;   break;
         case 4:  pintr->new_interrupt.pm_offset = (int) _int_handler_4;   break;
         case 5:  pintr->new_interrupt.pm_offset = (int) _int_handler_5;   break;
         case 6:  pintr->new_interrupt.pm_offset = (int) _int_handler_6;   break;
         case 7:  pintr->new_interrupt.pm_offset = (int) _int_handler_7;   break;
         default: return -1;
         }

         pintr->new_interrupt.pm_offset = (int) handler;

         pintr->handler = handler;
         pintr->irq_vector = chan;
         
         _go32_dpmi_get_protected_mode_interrupt_vector(pintr->irq_vector, &pintr->old_interrupt);
         _go32_dpmi_allocate_iret_wrapper(&pintr->new_interrupt);
         _go32_dpmi_set_protected_mode_interrupt_vector(pintr->irq_vector, &pintr->new_interrupt);

         return 0;
      }
   }
 
   return -1; /* none free */
}

void thin_int_remove(int chan)
{
   int i;

   for (i = 0; i < MAX_INTR; i++)
   {
      intr_t *pintr = &intr[i];
      if (pintr->irq_vector == chan && pintr->handler != NULL)
      {
         _go32_dpmi_set_protected_mode_interrupt_vector(pintr->irq_vector, &pintr->old_interrupt);
         _go32_dpmi_free_iret_wrapper(&pintr->new_interrupt);

         pintr->handler = NULL;
         pintr->irq_vector = 0;
         break;
      }
   }
}

/* helper routines for the irq masking */
static void _irq_exit(void)
{
   if (pic_modified)
   {
      pic_modified = false;
      outportb(0x21, pic1_orig);
      outportb(0xA1, pic2_orig);
      thin_remove_exit(_irq_exit);
   }
}

static void _irq_init(void)
{
   if (false == pic_modified)
   {
      pic_modified = true;

      /* read initial PIC values */
      pic1_orig = inportb(0x21);
      pic2_orig = inportb(0xA1);

      pic1_mask = 0;
      pic2_mask = 0;

      thin_add_exit(_irq_exit);
   }
}


/* restore original mask for interrupt */
void thin_irq_restore(int irq)
{
   if (pic_modified)
   {
      uint8 pic;

      if (irq > 7)
      {
         pic = inportb(0xA1) & ~(1 << (irq - 8));
         outportb(0xA1, pic | (pic2_orig & (1 << (irq - 8))));
         pic2_mask &= ~(1 << (irq - 8));

         if (pic2_mask)
            return;

         irq = 2; /* restore cascade if no high IRQs remain */
      }

      pic = inportb(0x21) & ~(1 << irq);
      outportb(0x21, pic | (pic1_orig & (1 << irq)));
      pic1_mask &= ~(1 << irq);
   }
}

/* unmask an interrupt */
void thin_irq_enable(int irq)
{
   uint8 pic;

   _irq_init();

   pic = inportb(0x21);

   if (irq > 7)
   {
      /* unmask cascade (IRQ2) interrupt */
      //outportb(0x21, pic & 0xFB);
      outportb(0x21, pic & ~(1 << 2));
      pic = inportb(0xA1);
      outportb(0xA1, pic & ~(1 << (irq - 8)));
      pic2_mask |= 1 << (irq - 8);
   }
   else
   {
      outportb(0x21, pic & ~(1 << irq));
      pic1_mask |= 1 << irq;
   }
}

/* mask an interrupt */
void thin_irq_disable(int irq)
{
   uint8 pic;

   _irq_init();

   if (irq > 7)
   {
      /* PIC 2 */
      pic = inportb(0xA1);
      outportb(0xA1, pic & (1 << (irq - 8)));
      pic2_mask |= 1 << (irq - 8);
   }
   else
   {
      /* PIC 1 */
      pic = inportb(0x21);
      outportb(0x21, pic & (1 << irq));
      pic1_mask |= 1 << irq;
   }
}


/*
** $Log: $
*/
