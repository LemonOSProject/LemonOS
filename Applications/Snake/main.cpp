#include <stdint.h>

#include <lemon/syscall.h>
#include <lemon/fb.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <gfx/graphics.h>
#include <lemon/filesystem.h>
#include <gfx/window/window.h>
#include <stdio.h>
#include <list.h>

int main(){

	win_info_t testWindow;
	testWindow.width = 200;
	testWindow.height = 100;
	testWindow.x = 10;
	testWindow.y = 150;
	strcpy(testWindow.title, "Snake");
	testWindow.flags = 0;

	Window* win = CreateWindow(&testWindow);

	PaintWindow(win);

	for(;;);
}
