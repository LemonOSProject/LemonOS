

/*
 * this code is not yet used. eventually we want to support using mmap
 * to map rom and sram into memory, so we don't waste virtual memory.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>


#include "defs.h"


#define DOTDIR ".gnuboy"

static char *home, *saves;
static char *romfile, *sramfile, *saveprefix;

static int mmapped_rom, mmaped_sram;


byte *map_rom()
{
	int fd, len;
	byte code;
	byte *mem;

	fd = open(romfile, O_READ);
	lseek(fd, 0x0148, SEEK_SET);
	read(fd, &code, 1);
	len = loader_romsize(code);
	
#ifdef HAVE_MMAP
	
	mem = mmap(0, len, PROT_READ, 
#endif
	
}





int map_checkdirs()
{
	home = malloc(strlen(getenv("HOME")) + strlen(DOTDIR) + 2);
	sprintf(home, "%s/" DOTDIR, getenv(HOME));
	saves = malloc(strlen(home) + 6);
	sprintf(saves, "%s/saves", home);
	
	if (access(saves, X_OK|W_OK))
	{
		if (access(home, X_OK|W_OK))
		{
			if (!access(home, F_OK))
				die("cannot access %s (%s)\n", home, strerror(errno));
			if (mkdir(home, 0777))
				die("cannot create %s (%s)\n", home, strerror(errno));
		}
		if (!access(saves, F_OK))
			die("cannot access %s (%s)\n", home, strerror(errno));
		if (mkdir(saves, 0777))
			die("cannot create %s (%s)\n", saves, strerror(errno));
	}
	return 0;
}










