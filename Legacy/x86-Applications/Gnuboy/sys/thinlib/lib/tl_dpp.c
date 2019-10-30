/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_dpp.c
**
** DOS DirectPad Pro scanning code, based on code from
** DirectPad Pro (www.ziplabel.com), written by Earle F. Philhower, III
**
** $Id: $
*/

#include <pc.h>

#include "tl_types.h"
#include "tl_dpp.h"
#include "tl_event.h"

#define  MAX_PADS    5

#define  NES_PWR     (0x80 + 0x40 + 0x20 + 0x10 + 0x08)
#define  NES_CLK     1
#define  NES_LAT     2
#define  NES_IN      ((inportb(port + 1) & nes_din) ^ xor_val)
#define  NES_OUT(v)  (outportb(port, (v)))

static const uint8 din_table[MAX_PADS] = { 0x40, 0x20, 0x10, 0x08, 0x80 };
static const int xor_table[MAX_PADS] = { 1, 1, 1, 1, 0 };

static dpp_t dpp[MAX_PADS];
static event_id dpp_id; /* event callback id */

void _dpp_poll(void)
{
   int i;
   int nes_din, port, xor_val;
   dpp_t *pad, old;
   thin_event_t event;

   for (i = 0; i < MAX_PADS; i++)
   {
      if (0 == dpp[i].port)
         continue;

      nes_din = din_table[i];
      port = dpp[i].port;
      xor_val = nes_din * xor_table[i];
      pad = &dpp[i];
      old = *pad;

      NES_OUT(NES_PWR);
      NES_OUT(NES_PWR + NES_LAT + NES_CLK);
      NES_OUT(NES_PWR);
      pad->a = NES_IN;

      NES_OUT(NES_PWR);
      NES_OUT(NES_PWR + NES_CLK);
      NES_OUT(NES_PWR);
      pad->b = NES_IN;

      NES_OUT(NES_PWR);
      NES_OUT(NES_PWR + NES_CLK);
      NES_OUT(NES_PWR);
      pad->select = NES_IN;

      NES_OUT(NES_PWR);
      NES_OUT(NES_PWR + NES_CLK);
      NES_OUT(NES_PWR);
      pad->start = NES_IN;

      NES_OUT(NES_PWR);
      NES_OUT(NES_PWR + NES_CLK);
      NES_OUT(NES_PWR);
      pad->up = NES_IN;

      NES_OUT(NES_PWR);
      NES_OUT(NES_PWR + NES_CLK);
      NES_OUT(NES_PWR);
      pad->down = NES_IN;

      NES_OUT(NES_PWR);
      NES_OUT(NES_PWR + NES_CLK);
      NES_OUT(NES_PWR);
      pad->left = NES_IN;

      NES_OUT(NES_PWR);
      NES_OUT(NES_PWR + NES_CLK);
      NES_OUT(NES_PWR);
      pad->right = NES_IN;

      NES_OUT(0); /* power down */

      /* generate some events if necessary */
      if (pad->left != old.left)
      {
         event.type = THIN_JOY_MOTION;
         event.data.joy_motion.dir = THIN_JOY_LEFT;
         event.data.joy_motion.state = pad->left;
         thin_event_add(&event);
      }

      if (pad->right != old.right)
      {
         event.type = THIN_JOY_MOTION;
         event.data.joy_motion.dir = THIN_JOY_RIGHT;
         event.data.joy_motion.state = pad->right;
         thin_event_add(&event);
      }

      if (pad->up != old.up)
      {
         event.type = THIN_JOY_MOTION;
         event.data.joy_motion.dir = THIN_JOY_UP;
         event.data.joy_motion.state = pad->up;
         thin_event_add(&event);
      }

      if (pad->down != old.down)
      {
         event.type = THIN_JOY_MOTION;
         event.data.joy_motion.dir = THIN_JOY_DOWN;
         event.data.joy_motion.state = pad->down;
         thin_event_add(&event);
      }

      if (pad->select != old.select)
      {
         event.type = pad->select ? THIN_JOY_BUTTON_PRESS : THIN_JOY_BUTTON_RELEASE;
         event.data.joy_button = 2;
         thin_event_add(&event);
      }

      if (pad->start != old.start)
      {
         event.type = pad->start ? THIN_JOY_BUTTON_PRESS : THIN_JOY_BUTTON_RELEASE;
         event.data.joy_button = 3;
         thin_event_add(&event);
      }

      if (pad->b != old.b)
      {
         event.type = pad->b ? THIN_JOY_BUTTON_PRESS : THIN_JOY_BUTTON_RELEASE;
         event.data.joy_button = 0;
         thin_event_add(&event);
      }

      if (pad->a != old.a)
      {
         event.type = pad->a ? THIN_JOY_BUTTON_PRESS : THIN_JOY_BUTTON_RELEASE;
         event.data.joy_button = 1;
         thin_event_add(&event);
      }
   }
}

void thin_dpp_read(dpp_t *pad, int pad_num)
{
   *pad = dpp[pad_num];
}

int thin_dpp_add(uint16 port, int pad_num)
{
   dpp[pad_num].port = port;

   return 0;
}

int thin_dpp_init(void)
{
   dpp_id = thin_event_add_callback((event_callback_t) _dpp_poll);
   if (-1 == dpp_id)
      return -1;

   return 0;
}

void thin_dpp_shutdown(void)
{
   if (-1 != dpp_id)
   {
      thin_event_remove_callback(dpp_id);
      dpp_id = -1;

      memset(dpp, 0, sizeof(dpp));
   }
}


/*
** $Log: $
*/
