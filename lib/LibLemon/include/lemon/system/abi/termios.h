#pragma once

#include <asm/ioctls.h>
#include <abi-bits/termios.h>

struct winsz {
	unsigned short rowCount; // Rows (in characters)
	unsigned short colCount; // Columns (in characters)
	unsigned short width; // Width (in pixels)
	unsigned short height; // Height (in pixels)
};

#define TCIFLUSH 1
#define TCIOFLUSH 2
#define TCOFLUSH 3

#define TIOCGPGRP 0x540F
#define TIOCSPGRP 0x5410
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define TIOCGSID 0x5429
