/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_joy.c
**
** DOS joystick reading routines
**
** $Id: $
*/

#include <dos.h>

#include "tl_types.h"
#include "tl_int.h"
#include "tl_joy.h"
#include "tl_event.h"

#define  JOY_PORT          0x201
#define  JOY_TIMEOUT       10000

#define  J1_A              0x10
#define  J1_B              0x20
#define  J2_A              0x40
#define  J2_B              0x80

#define  J1_X              0x01
#define  J1_Y              0x02
#define  J2_X              0x04
#define  J2_Y              0x08

#define  JOY_CENTER        0xAA
#define  JOY_MIN_THRESH    0.7
#define  JOY_MAX_THRESH    1.3

static struct joyinfo_s
{
   int id; /* callback ID */
   int x_minthresh, x_maxthresh;
   int y_minthresh, y_maxthresh;
   int x_read, y_read;
   uint8 button_state;
   bool disconnected;
} joyinfo;

static joy_t joystick;

/* Read data in from joy port */
static int _portread(void)
{
   /* Set timeout to max number of samples */
   int timeout = JOY_TIMEOUT;
   uint8 port_val;

   joyinfo.x_read = 0;
   joyinfo.y_read = 0;

   THIN_DISABLE_INTS();

   /* Clear the latch and request a sample */
   port_val = inportb(JOY_PORT);
   outportb(JOY_PORT, port_val);

   do
   {
      port_val = inportb(JOY_PORT);
      if (port_val & J1_X)
         joyinfo.x_read++;
      if (port_val & J1_Y)
         joyinfo.y_read++;
   }
   while (--timeout && (port_val & 3));

   joyinfo.button_state = port_val;

   THIN_ENABLE_INTS();

   if (0 == timeout)
      return -1;
   else
      return 0;
}

void _poll_joystick(void)
{
   int i;
   joy_t old;
   thin_event_t event;

   old = joystick;

   if (_portread())
   {
      joyinfo.disconnected = true;
      return;
   }

   /* Calc X axis */
   joystick.left = (joyinfo.x_read < joyinfo.x_minthresh) ? true : false;
   joystick.right = (joyinfo.x_read > joyinfo.x_maxthresh) ? true : false;

   /* Calc Y axis */
   joystick.up = (joyinfo.y_read < joyinfo.y_minthresh) ? true : false;
   joystick.down = (joyinfo.y_read > joyinfo.y_maxthresh) ? true : false;

   /* Get button status */
   /* note that buttons returned by hardware are inverted logic */
   joystick.button[0] = (joyinfo.button_state & J1_A) ? false : true;
   joystick.button[1] = (joyinfo.button_state & J2_A) ? false : true;
   joystick.button[2] = (joyinfo.button_state & J1_B) ? false : true;
   joystick.button[3] = (joyinfo.button_state & J2_B) ? false : true;

   /* generate some events if necessary */
   if (joystick.left != old.left)
   {
      event.type = THIN_JOY_MOTION;
      event.data.joy_motion.dir = THIN_JOY_LEFT;
      event.data.joy_motion.state = joystick.left;
      thin_event_add(&event);
   }

   if (joystick.right != old.right)
   {
      event.type = THIN_JOY_MOTION;
      event.data.joy_motion.dir = THIN_JOY_RIGHT;
      event.data.joy_motion.state = joystick.right;
      thin_event_add(&event);
   }

   if (joystick.up != old.up)
   {
      event.type = THIN_JOY_MOTION;
      event.data.joy_motion.dir = THIN_JOY_UP;
      event.data.joy_motion.state = joystick.up;
      thin_event_add(&event);
   }

   if (joystick.down != old.down)
   {
      event.type = THIN_JOY_MOTION;
      event.data.joy_motion.dir = THIN_JOY_DOWN;
      event.data.joy_motion.state = joystick.down;
      thin_event_add(&event);
   }

   for (i = 0; i < JOY_MAX_BUTTONS; i++)
   {
      if (joystick.button[i] != old.button[i])
      {
         event.type = joystick.button[i] ? THIN_JOY_BUTTON_PRESS : THIN_JOY_BUTTON_RELEASE;
         event.data.joy_button = i;

         thin_event_add(&event);
      }
   }
}

int thin_joy_read(joy_t *joy)
{
   if (joyinfo.disconnected)
      return -1;

   *joy = joystick;

   return 0;
}

/* Detect presence of joystick */
int thin_joy_init(void)
{
   joyinfo.disconnected = true;

   if (_portread())
      return -1;

   joyinfo.id = thin_event_add_callback((event_callback_t) _poll_joystick);
   if (-1 == joyinfo.id)
      return -1;

   joyinfo.disconnected = false;

   /* Set the threshhold */
   joyinfo.x_minthresh = JOY_MIN_THRESH * joyinfo.x_read;
   joyinfo.x_maxthresh = JOY_MAX_THRESH * joyinfo.x_read;
   joyinfo.y_minthresh = JOY_MIN_THRESH * joyinfo.y_read;
   joyinfo.y_maxthresh = JOY_MAX_THRESH * joyinfo.y_read;

   return 0;
}

void thin_joy_shutdown(void)
{
   joyinfo.disconnected = true;

   if (-1 != joyinfo.id)
   {
      thin_event_remove_callback(joyinfo.id);
      joyinfo.id = -1;
   }
}

/*
** $Log: $
*/
