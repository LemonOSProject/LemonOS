/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_key.c
**
** DOS keyboard handler
**
** $Id: $
*/

#include <stdlib.h>
#include <go32.h>
#include <dpmi.h>
#include <dos.h>

#include "tl_types.h"
#include "tl_djgpp.h"
#include "tl_key.h"
#include "tl_event.h"

#define  KEYBOARD_INT      0x09

/* maybe make this globally accessible? */
static int key_status[THIN_MAX_KEYS];
static bool key_repeat = false;
static bool ext_key = false;

static _go32_dpmi_seginfo old_key_handler;
static _go32_dpmi_seginfo new_key_handler;


static const uint8 ext_tab[0x80] = 
{
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x00 - 0x07 */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x08 - 0x0F */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x10 - 0x17 */
   0, 0, 0, 0, THIN_KEY_NUMPAD_ENTER, THIN_KEY_RIGHT_CTRL, 0, 0,  /* 0x10 - 0x1F */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x20 - 0x27 */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x28 - 0x2F */
   0, 0, 0, 0, 0, THIN_KEY_NUMPAD_DIV, 0, THIN_KEY_SYSRQ, /* 0x30 - 0x37 */
   THIN_KEY_RIGHT_ALT, 0, 0, 0, 0, 0, 0, 0, /* 0x38 - 0x3F */
   0, 0, 0, 0, 0, 0, THIN_KEY_BREAK, THIN_KEY_HOME, /* 0x40 - 0x47 */
   THIN_KEY_UP, THIN_KEY_PGUP, 0, THIN_KEY_LEFT, 0, THIN_KEY_RIGHT, 0, THIN_KEY_END, /* 0x48 - 0x4F */
   THIN_KEY_DOWN, THIN_KEY_PGDN, THIN_KEY_INSERT, THIN_KEY_DELETE, /* 0x50 - 0x57 */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x58 - 0x5F */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x60 - 0x67 */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x68 - 0x6F */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 - 0x77 */
   0, 0, 0, 0, 0, 0, 0, 0, /* 0x78 - 0x7F */
};

/* keyboard ISR */
static void key_handler(void)
{
   thin_event_t event;
   uint8 raw_code, ack_code;

   /* read the key */
   raw_code = inportb(0x60);
   ack_code = inportb(0x61);
   outportb(0x61, ack_code | 0x80);
   outportb(0x61, ack_code);
   outportb(0x20, 0x20);

   if (0xE0 == raw_code)
   {
      ext_key = true;
   }
   else
   {
      if (ext_key)
      {
         ext_key = false;
         event.data.keysym = ext_tab[raw_code & 0x7F];
      }
      else
      {
         event.data.keysym = raw_code & 0x7F;
      }

      event.type = (raw_code & 0x80) ? THIN_KEY_RELEASE : THIN_KEY_PRESS;

      if (key_repeat || (event.type != key_status[event.data.keysym]))
      {
         key_status[event.data.keysym] = event.type;
         thin_event_add(&event);
      }
   }
}
THIN_LOCKED_STATIC_FUNC(key_handler)

void thin_key_set_repeat(bool state)
{
   key_repeat = state;
}

/* set up variables, lock code/data, set the new handler and save old one */
int thin_key_init(void)
{
   THIN_LOCK_FUNC(key_handler);
   THIN_LOCK_VAR(key_status);
   THIN_LOCK_VAR(ext_key);
   
   _go32_dpmi_get_protected_mode_interrupt_vector(KEYBOARD_INT, &old_key_handler);
   new_key_handler.pm_offset = (uint32) key_handler;
   new_key_handler.pm_selector = _go32_my_cs();
   _go32_dpmi_allocate_iret_wrapper(&new_key_handler);
   _go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARD_INT, &new_key_handler);

   memset(key_status, THIN_KEY_RELEASE, sizeof(key_status));

   return 0; /* can't fail */
}

/* restore old keyboard handler */
void thin_key_shutdown(void)
{
   _go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARD_INT, &old_key_handler);
   _go32_dpmi_free_iret_wrapper(&new_key_handler);
}

/*
** $Log: $
*/
