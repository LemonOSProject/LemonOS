/*
 * dos.c
 *
 * System interface for DOS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char *strdup();
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define US(n) ( ((long long)(n)) * 1000000 / UCLOCKS_PER_SEC )


void *sys_timer()
{
	uclock_t *cl;
	
	cl = malloc(sizeof *cl);
	*cl = uclock();
	return cl;
}

int sys_elapsed(uclock_t *cl)
{
	uclock_t now;
	int usecs;

	now = uclock();
	usecs = US(now - *cl);
	*cl = now;
	return usecs;
}

void sys_sleep(int us)
{
	uclock_t start;
	if (us <= 0) return;
	start = uclock();
	while(US(uclock()-start) < us);
}

void sys_checkdir(char *path, int wr)
{
}

void sys_initpath(char *exe)
{
	char *buf, *home, *p;

	home = strdup(exe);
	p = strrchr(home, '/');
	if (p) *p = 0;
	else
	{
		buf = ".";
		rc_setvar("rcpath", 1, &buf);
		rc_setvar("savedir", 1, &buf);
		return;
	}
	buf = malloc(strlen(home) + 8);
	sprintf(buf, ".;%s/", home);
	rc_setvar("rcpath", 1, &buf);
	sprintf(buf, ".");
	rc_setvar("savedir", 1, &buf);
	free(buf);
}

void sys_sanitize(char *s)
{
	int i;
	for (i = 0; s[i]; i++)
		if (s[i] == '\\') s[i] = '/';
}






