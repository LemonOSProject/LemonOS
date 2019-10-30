/*
** thinlib (c) 2001 Matthew Conte (matt@conte.com)
**
**
** tl_event.c
**
** event handling routines
**
** $Id: $
*/

#include "tl_types.h"
#include "tl_event.h"
#include "tl_djgpp.h"


/* maximum of 8 event handling callbacks */
#define  MAX_CALLBACKS     8


#define  EVENT_QUEUE_MAX   256
#define  EVENT_QUEUE_MASK  (EVENT_QUEUE_MAX - 1)
#define  EVENT_QUEUE_EMPTY (event_queue.head == event_queue.tail)

typedef struct event_queue_s
{
   int head;
   int tail;
   thin_event_t event[EVENT_QUEUE_MAX];
} event_queue_t;


static event_queue_t event_queue;
static event_callback_t event_callback[MAX_CALLBACKS];


/* add an event. */
void thin_event_add(thin_event_t *event)
{
   event_queue.event[event_queue.head] = *event;
   event_queue.head = (event_queue.head + 1) & EVENT_QUEUE_MASK;
}
THIN_LOCKED_FUNC(thin_event_add)


/* get an event from the event queue.  returns 0 if no events. */
int thin_event_get(thin_event_t *event)
{
   if (EVENT_QUEUE_EMPTY)
   {
      event->type = THIN_NOEVENT;
      return 0;
   }

   *event = event_queue.event[event_queue.tail];
   event_queue.tail = (event_queue.tail + 1) & EVENT_QUEUE_MASK;

   return 1;
}


/* gather up all pollable events */
void thin_event_gather(void)
{
   int i;

   for (i = 0; i < MAX_CALLBACKS; i++)
   {
      if (NULL == event_callback[i])
         return;

      event_callback[i]();
   }
}


/* return an ID of an event callback */
event_id thin_event_add_callback(event_callback_t callback)
{
   event_id id;

   for (id = 0; id < MAX_CALLBACKS; id++)
   {
      if (NULL == event_callback[id])
         break;
   }

   /* no event callbacks available */
   if (id == MAX_CALLBACKS)
      return (event_id) -1;

   event_callback[id] = callback;

   return id;
}


/* remove an event callback */
void thin_event_remove_callback(event_id id)
{
   THIN_ASSERT(id >= 0 && id < MAX_CALLBACKS);

   if (id < 0 || id >= MAX_CALLBACKS)
      return;

   THIN_ASSERT(NULL != event_callback[id]);

   event_callback[id] = NULL;

   /* move all other callbacks down */
   for (; id < MAX_CALLBACKS - 1; id++)
   {
      event_callback[id] = event_callback[id + 1];
      event_callback[id + 1] = NULL;
   }
}


/* set up the event handling system */
void thin_event_init(void)
{
   int i;

   /* some modules call thin_event_add from an ISR, so we must
   ** lock everythig that is touched within that function, as
   ** well as the code itself.
   */
   THIN_LOCK_FUNC(thin_event_add);
   THIN_LOCK_VAR(event_queue);

   for (i = 0; i < MAX_CALLBACKS; i++)
      event_callback[i] = NULL;

   event_queue.head = event_queue.tail = 0;
}


/*
** $Log: $
*/
