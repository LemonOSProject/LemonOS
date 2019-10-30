#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <linux/joystick.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "input.h"
#include "rc.h"

static int usejoy = 1;
static char *joydev;
static int joyfd = -1;
static int pos[2], max[2], min[2];
static const int axis[2][2] =
{
	{ K_JOYLEFT, K_JOYRIGHT },
	{ K_JOYUP, K_JOYDOWN }
};

rcvar_t joy_exports[] =
{
	RCV_BOOL("joy", &usejoy),
	RCV_STRING("joy_device", &joydev),
	RCV_END
};




void joy_init()
{
	int set;
	if (!usejoy) return;
	set = 0;
	if (!joydev) {
		joydev = strdup("/dev/js0");
		set = 1;
	}
	joyfd = open(joydev, O_RDONLY|O_NONBLOCK);
	if(joyfd == -1 && set) {
		free(joydev);
		joydev = strdup("/dev/input/js0");
		joyfd = open(joydev, O_RDONLY|O_NONBLOCK);
	}
}

void joy_close()
{
	close(joyfd);
}

void joy_poll()
{
	struct js_event js;
	event_t ev;
	int n;
	
	if (joyfd < 0) return;

	while (read(joyfd,&js,sizeof(struct js_event)) == sizeof(struct js_event))
	{
		switch(js.type)
		{
		case JS_EVENT_BUTTON:
			ev.type = js.value ? EV_PRESS : EV_RELEASE;
			ev.code = K_JOY0 + js.number;
			ev_postevent(&ev);
			break;
		case JS_EVENT_AXIS:
			n = js.number & 1;
			if (js.value < min[n]) min[n] = js.value;
			else if(js.value > max[n]) max[n] = js.value;
			ev.code = axis[n][0];
			if(js.value < (min[n]>>2) && js.value < pos[n])
			{
				ev.type = EV_PRESS;
				ev_postevent(&ev);
			}
			else if (js.value > pos[n])
			{
				ev.type = EV_RELEASE;
				ev_postevent(&ev);
			}
			ev.code = axis[n][1];
			if(js.value > (max[n]>>2) && js.value > pos[n])
			{
				ev.type = EV_PRESS;
				ev_postevent(&ev);
			}
			else if (js.value < pos[n])
			{
				ev.type = EV_RELEASE;
				ev_postevent(&ev);
			}
			pos[n] = js.value;
		}
	}
}

#ifdef JOY_TEST

#include "../../events.c"
#include "../../keytable.c"

int main() {
	joy_init();
	event_t e;
	while(1) {
		joy_poll();
		if(ev_getevent(&e)) {
			static const char* evt_str[] = {
				[EV_NONE] = "none",
				[EV_PRESS] = "press",
				[EV_RELEASE]  = "release",
				[EV_REPEAT] = "repeat",
				[EV_MOUSE] = "mouse",
			};
			printf("%s: %s\n", evt_str[e.type], k_keyname(e.code));
		} else {
			usleep(300);
		}
	}
}

#endif
