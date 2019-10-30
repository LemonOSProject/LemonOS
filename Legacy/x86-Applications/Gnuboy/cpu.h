

#ifndef __CPU_H__
#define __CPU_H__



#include "defs.h"


union reg
{
	byte b[2][2];
	word w[2];
	un32 d; /* padding for alignment, carry */
};

struct cpu
{
	union reg pc, sp, bc, de, hl, af;
	int ime, ima;
	int speed;
	int halt;
	int div, tim;
	int lcdc;
	int snd;
};

extern struct cpu cpu;

void cpu_reset();
void div_advance(int cnt);
void timer_advance(int cnt);
void lcdc_advance(int cnt);
void sound_advance(int cnt);
void cpu_timers(int cnt);
int cpu_emulate(int cycles);

#endif


