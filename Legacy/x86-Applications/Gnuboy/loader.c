#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "defs.h"
#include "regs.h"
#include "mem.h"
#include "hw.h"
#include "rtc.h"
#include "rc.h"
#include "lcd.h"
#include "inflate.h"
#include "xz/xz.h"
#include "save.h"
#include "sound.h"
#include "sys.h"

static int mbc_table[256] =
{
	0, 1, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 3,
	3, 3, 3, 3, 0, 0, 0, 0, 0, 5, 5, 5, MBC_RUMBLE, MBC_RUMBLE, MBC_RUMBLE, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MBC_HUC3, MBC_HUC1
};

static int rtc_table[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static int batt_table[256] =
{
	0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0,
	1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
	0
};

static int romsize_table[256] =
{
	2, 4, 8, 16, 32, 64, 128, 256, 512,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 128, 128, 128
	/* 0, 0, 72, 80, 96  -- actual values but bad to use these! */
};

static int ramsize_table[256] =
{
	1, 1, 1, 4, 16,
	4 /* FIXME - what value should this be?! */
};


static char *romfile;
static char *sramfile;
static char *rtcfile;
static char *saveprefix;

static char *savename;
static char *savedir;

static int saveslot;

static int forcebatt, nobatt;
static int forcedmg, gbamode;

static int memfill = -1, memrand = -1;


static void initmem(void *mem, int size)
{
	char *p = mem;
	if (memrand >= 0)
	{
		srand(memrand ? memrand : time(0));
		while(size--) *(p++) = rand();
	}
	else if (memfill >= 0)
		memset(p, memfill, size);
}

static byte *loadfile(int f, int *len)
{
	/*int c, l = 0, p = 0;
	byte *d = 0, buf[512];
	
	for(;;)
	{
		c = fread(buf, 1, 512, f);
		if (c <= 0) break;
		l += c;
		d = realloc(d, l);
		if (!d) return 0;
		memcpy(d+p, buf, c);
		p += c;
	}
	*len = l;
	return d;*/
	*len = lseek(f,0,SEEK_END);
	uint8_t* data = (uint8_t*)malloc(*len);
	if(!read(f, data, *len))return 0;
	return data;
}

static byte *inf_buf;
static int inf_pos, inf_len;
/*
static void inflate_callback(byte b)
{
	if (inf_pos >= inf_len)
	{
		inf_len += 512;
		inf_buf = realloc(inf_buf, inf_len);
		if (!inf_buf) die("out of memory inflating file @ %d bytes\n", inf_pos);
	}
	inf_buf[inf_pos++] = b;
}

static byte *gunzip(byte *data, int *len) {
	long pos = 0;
	inf_buf = 0;
	inf_pos = inf_len = 0;
	if (unzip(data, &pos, inflate_callback) < 0)
		return data;
	*len = inf_pos;
	return inf_buf;
}

static void write_dec(byte *data, int len) {
	int i;
	for(i=0; i < len; i++)
		inflate_callback(data[i]);
}
/*
static int unxz(byte *data, int len) {
	struct xz_buf b;
	struct xz_dec *s;
	enum xz_ret ret;
	unsigned char out[4096];

	/*
	 * Support up to 64 MiB dictionary. The actually needed memory
	 * is allocated once the headers have been parsed.
	* /
	s = xz_dec_init(XZ_DYNALLOC, 1 << 26);
	if(!s) goto err;

	b.in = data;
	b.in_pos = 0;
	b.in_size = len;
	b.out = out;
	b.out_pos = 0;
	b.out_size = sizeof(out);

	while (1) {
		ret = xz_dec_run(s, &b);
		if(b.out_pos == sizeof(out)) {
			write_dec(out, sizeof(out));
			b.out_pos = 0;
		}

		if(ret == XZ_OK) continue;

		write_dec(out, b.out_pos);

		if(ret == XZ_STREAM_END) {
			xz_dec_end(s);
			return 0;
		}
		goto err;
	}

	err:
	xz_dec_end(s);
	return -1;
}

static byte *do_unxz(byte *data, int *len) {
	xz_crc32_init();
	xz_crc64_init();
	inf_buf = 0;
	inf_pos = inf_len = 0;
	if (unxz(data, *len) < 0)
		return data;
	*len = inf_pos;
	return inf_buf;
}

static byte *decompress(byte *data, int *len)
{
	if (data[0] == 0x1f && data[1] == 0x8b)
		return gunzip(data, len);
	if(data[0] == 0xFD && !strncmp(data+1, "7zXZ", 4))
		return do_unxz(data, len);
	return data;
}*/


int rom_load()
{
	//FILE *f;
	int f;
	byte c, *data, *header;
	int len = 0, rlen;

	//if (strcmp(romfile, "-")) f = fopen(romfile, "rb");
	//else f = stdin;
	f = open(romfile, "rb");
	if (!f) die("cannot open rom file: %s\n", romfile);

	data = loadfile(f, &len);
	header = data ;//= decompress(data, &len);
	
	memcpy(rom.name, header+0x0134, 16);
	
	if (rom.name[14] & 0x80) rom.name[14] = 0;
	if (rom.name[15] & 0x80) rom.name[15] = 0;
	rom.name[16] = 0;

	c = header[0x0147];
	mbc.type = mbc_table[c];
	mbc.batt = (batt_table[c] && !nobatt) || forcebatt;
	rtc.batt = rtc_table[c];
	mbc.romsize = romsize_table[header[0x0148]];
	mbc.ramsize = ramsize_table[header[0x0149]];

	if (!mbc.romsize) die("unknown ROM size %02X\n", header[0x0148]);
	if (!mbc.ramsize) die("unknown SRAM size %02X\n", header[0x0149]);

	rlen = 16384 * mbc.romsize;
	rom.bank = realloc(data, rlen);
	if (rlen > len) memset(rom.bank[0]+len, 0xff, rlen - len);
	
	ram.sbank = malloc(8192 * mbc.ramsize);

	initmem(ram.sbank, 8192 * mbc.ramsize);
	initmem(ram.ibank, 4096 * 8);

	mbc.rombank = 1;
	mbc.rambank = 0;

	c = header[0x0143];
	hw.cgb = ((c == 0x80) || (c == 0xc0)) && !forcedmg;
	hw.gba = (hw.cgb && gbamode);

	if (strcmp(romfile, "-")) close(f);

	return 0;
}

int sram_load()
{
	FILE *f;

	if (!mbc.batt || !sramfile || !*sramfile) return -1;

	/* Consider sram loaded at this point, even if file doesn't exist */
	ram.loaded = 1;

	f = fopen(sramfile, "rb");
	if (!f) return -1;
	fread(ram.sbank, 8192, mbc.ramsize, f);
	fclose(f);
	
	return 0;
}


int sram_save()
{
	FILE *f;

	/* If we crash before we ever loaded sram, DO NOT SAVE! */
	if (!mbc.batt || !sramfile || !ram.loaded || !mbc.ramsize)
		return -1;
	
	f = fopen(sramfile, "wb");
	if (!f) return -1;
	fwrite(ram.sbank, 8192, mbc.ramsize, f);
	fclose(f);
	
	return 0;
}


void state_save(int n)
{
	FILE *f;
	char *name;

	if (n < 0) n = saveslot;
	if (n < 0) n = 0;
	name = malloc(strlen(saveprefix) + 5);
	sprintf(name, "%s.%03d", saveprefix, n);

	if ((f = fopen(name, "wb")))
	{
		savestate(f);
		fclose(f);
	}
	free(name);
}


void state_load(int n)
{
	FILE *f;
	char *name;

	if (n < 0) n = saveslot;
	if (n < 0) n = 0;
	name = malloc(strlen(saveprefix) + 5);
	sprintf(name, "%s.%03d", saveprefix, n);

	if ((f = fopen(name, "rb")))
	{
		loadstate(f);
		fclose(f);
		vram_dirty();
		pal_dirty();
		sound_dirty();
		mem_updatemap();
	}
	free(name);
}

void rtc_save()
{
	FILE *f;
	if (!rtc.batt) return;
	if (!(f = fopen(rtcfile, "wb"))) return;
	rtc_save_internal(f);
	fclose(f);
}

void rtc_load()
{
	FILE *f;
	if (!rtc.batt) return;
	if (!(f = fopen(rtcfile, "r"))) return;
	rtc_load_internal(f);
	fclose(f);
}


void loader_unload()
{
	sram_save();
	if (romfile) free(romfile);
	if (sramfile) free(sramfile);
	if (saveprefix) free(saveprefix);
	if (rom.bank) free(rom.bank);
	if (ram.sbank) free(ram.sbank);
	romfile = sramfile = saveprefix = 0;
	rom.bank = 0;
	ram.sbank = 0;
	mbc.type = mbc.romsize = mbc.ramsize = mbc.batt = 0;
}

static char *base(char *s)
{
	char *p;
	p = strrchr(s, '/');
	if (p) return p+1;
	return s;
}

static char *ldup(char *s)
{
	int i;
	char *n, *p;
	p = n = malloc(strlen(s));
	for (i = 0; s[i]; i++) if (isalnum(s[i])) *(p++) = tolower(s[i]);
	*p = 0;
	return n;
}

static void cleanup()
{
	sram_save();
	rtc_save();
	/* IDEA - if error, write emergency savestate..? */
}

void loader_init(char *s)
{
	char *name, *p;

	//sys_checkdir(savedir, 1); /* needs to be writable */

	romfile = s;
	rom_load();
	//vid_settitle(rom.name);
	/*if (savename && *savename)
	{
		if (savename[0] == '-' && savename[1] == 0)
			name = ldup(rom.name);
		else name = strdup(savename);
	}
	else if (romfile && *base(romfile) && strcmp(romfile, "-"))
	{
		name = strdup(base(romfile));
		p = strchr(name, '.');
		if (p) *p = 0;
	}*/
	/*else*/ name = ldup(rom.name);
	
	//saveprefix = malloc(strlen(savedir) + strlen(name) + 2);
	//sprintf(saveprefix, "%s/%s", savedir, name);
/*
	sramfile = malloc(strlen(saveprefix) + 5);
	strcpy(sramfile, saveprefix);
	strcat(sramfile, ".sav");

	rtcfile = malloc(strlen(saveprefix) + 5);
	strcpy(rtcfile, saveprefix);
	strcat(rtcfile, ".rtc");*/
	
	//sram_load();
	//rtc_load();

	//atexit(cleanup);
}

rcvar_t loader_exports[] =
{
	RCV_STRING("savedir", &savedir),
	RCV_STRING("savename", &savename),
	RCV_INT("saveslot", &saveslot),
	RCV_BOOL("forcebatt", &forcebatt),
	RCV_BOOL("nobatt", &nobatt),
	RCV_BOOL("forcedmg", &forcedmg),
	RCV_BOOL("gbamode", &gbamode),
	RCV_INT("memfill", &memfill),
	RCV_INT("memrand", &memrand),
	RCV_END
};









