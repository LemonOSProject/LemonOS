#include <stdint.h>

#include <lemon/syscall.h>
#include <lemon/fb.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <gfx/graphics.h>
#include <lemon/filesystem.h>

uint8_t* fb = 0;

fb_info_t fbInfo;

vector2i_t mousePos = {0,0};
int main(){
	fb = lemon_map_fb(&fbInfo);
	
	surface_t* fbSurface = (surface_t*)malloc(sizeof(surface_t));
	//fbSurface = CreateFramebufferSurface(fbInfo,fb);
	fbSurface->x = fbSurface->y = 0;
	fbSurface->width = fbInfo.width;
	fbSurface->height = fbInfo.height;
	fbSurface->pitch = fbInfo.pitch;
	fbSurface->depth = fbInfo.bpp;

	for(int i = 0; i < fbInfo.height*fbInfo.pitch;i++){
		fb[i] = 0;
	}

	for(int i = 0; i < fbSurface->height*fbSurface->pitch;i++){
		fb[i] = 255;
	}
	char* a = "trsteaf";
	//DrawString(a,0,40,0,0,0,fbSurface);

	for(;;);

	int bg = lemon_open("/bg.bmp",0);

	int size = lemon_seek(bg, 0, 2);

	//uint8_t* buffer = (uint8_t*)malloc(size);
	//memset(buffer,0,size);
	//lemon_read(bg, buffer, size);

	//DrawBitmapImage(0,0,1024,768,buffer,fbSurface);
/*
	bitmap_file_header_t* fileHeader = (bitmap_file_header_t*)buffer;
	bitmap_info_header_t* infoHeader = (bitmap_info_header_t*)buffer + 14;//fileHeader->size;
	char test[16];
	memset(test,0,16);
	DrawString(itoa(fileHeader->offset,test,10),0,0,0,0,0,fbSurface);
	memset(test,0,16);
	DrawString(itoa(infoHeader->width,test,10),0,20,0,0,0,fbSurface);*/

	for(;;);
}