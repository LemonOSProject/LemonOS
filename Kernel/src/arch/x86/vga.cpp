#include <vga.h>
#include <system.h>

namespace VGA {
	uint8_t* video_memory = (uint8_t*)0xB8000 + KERNEL_VIRTUAL_BASE;

	uint16_t screen_width = 80;
	uint16_t screen_height = 25;
	uint16_t screen_depth = 2;

	uint16_t curX;
	uint16_t curY;

	void putc(char c, uint8_t colour) {
		switch (c) {
		case '\n':
			curX = 0;
			curY++;
			break;
		default:
			video_memory[(screen_width*curY + curX)*screen_depth] = c;
			video_memory[(screen_width*curY + curX)*screen_depth + 1] = colour;
			curX++;
			break;
		}
	}

	void puts(char* string, uint8_t colour) {
		int i = 0;
		while (string[i])
			putc(string[i++], colour);
	}

	void clearscreen(uint8_t colour) {
		curX = 0;
		curY = 0;

		for (int i = 0; i < screen_width * screen_height; i++)
			putc(' ', colour);

		curX = 0;
		curY = 0;
	}
}