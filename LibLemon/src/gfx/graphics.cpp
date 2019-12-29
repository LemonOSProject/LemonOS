#include <gfx/graphics.h>

#include <lemon/syscall.h>
#include <lemon/fb.h>

#include <string.h>
#include <stdlib.h>

extern "C" void memcpy_sse2(void* dest, void* src, size_t count);
extern "C" void memcpy_sse2_unaligned(void* dest, void* src, size_t count);
extern "C" void memset32_sse2(void* dest, uint32_t c, uint64_t count);
extern "C" void memset64_sse2(void* dest, uint64_t c, uint64_t count);

#ifdef Lemon32
void memcpy_optimized(void* dest, void* src, size_t count) {
	//if(((size_t)dest % 0x10) || ((size_t)src % 0x10)) memcpy(dest, src, count); 

	dest = dest; //+ 0x10-start_overflow;
	count -= 0;//0x10 - start_overflow;

	size_t overflow = (count % 0x10); // Amount of overflow bytes
	size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

    if(((size_t)dest % 0x10) || ((size_t)src % 0x10))
	    memcpy_sse2_unaligned(dest, src, size_aligned/0x10);
    else
	    memcpy_sse2(dest, src, size_aligned/0x10);

	if (overflow > 0)
		memcpy(dest + size_aligned, src + size_aligned, overflow);
}

void memset32_optimized(void* dest, uint32_t c, size_t count) {
	if(((size_t)dest % 0x10)){
        while(count--){
            *((uint32_t*)dest) = c;
            dest+=sizeof(uint32_t);
        }
        return;
    }

	size_t overflow = (count % 0x4); // Amount of overflow bytes
	size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

	memset_sse2(dest, c, size_aligned/0x4);

    while(overflow--){
            *((uint32_t*)dest) = c;
            dest+=sizeof(uint32_t);
        }
}
#else
void memset32_optimized(void* dest, uint32_t c, size_t count) {
	{//if(((size_t)dest % 0x10)){
        while(count--){
            *((uint32_t*)dest) = c;
            dest+=sizeof(uint32_t);
        }
        return;
    }

	size_t overflow = (count % 0x4); // Amount of overflow bytes
	size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

	memset32_sse2(dest, c, size_aligned/0x4);

    while(overflow--){
            *((uint32_t*)dest) = c;
            dest+=sizeof(uint32_t);
        }
}

void memset64_optimized(void* dest, uint64_t c, size_t count) {
	if(((size_t)dest % 0x10)){
        while(count--){
            *((uint64_t*)dest) = c;
            dest+=sizeof(uint64_t);
        }
        return;
    }

	size_t overflow = (count % 0x8); // Amount of overflow bytes
	size_t size_aligned = (count - overflow); // Size rounded DOWN to lowest multiple of 128 bits

	memset64_sse2(dest, c, size_aligned/0x8);

    while(overflow--){
            *((uint64_t*)dest) = c;
            dest+=sizeof(uint64_t);
        }
}
#endif

surface_t* CreateFramebufferSurface(fb_info_t fbInfo, void* address){
    surface_t* surface = (surface_t*)malloc(sizeof(surface_t));
    surface->x = surface->y = 0;
    surface->width = fbInfo.width;
    surface->height = fbInfo.height;
    surface->depth = fbInfo.bpp;
    surface->pitch = fbInfo.pitch;

    surface->buffer = (uint8_t*)address;

    return surface;
}

int floor(double num) {
	int x = (int)num;
	return num < x ? x - 1 : x;
}

void DrawRect(rect_t rect, rgba_colour_t colour, surface_t* surface){
    if(rect.pos.x < 0){
        rect.size.x += rect.pos.x;
        rect.pos.x = 0;
    }

    if(rect.pos.y < 0){
        rect.size.y += rect.pos.y;
        rect.pos.y = 0;
    }
    
    uint32_t colour_i = (colour.a << 24) | (colour.r << 16) | (colour.g << 8) | colour.b;
    uint32_t* buffer = (uint32_t*)surface->buffer; // Convert byte array into an array of 32-bit unsigned integers as the supported colour depth is 32 bit
    for(int i = 0; i < rect.size.y && i + rect.pos.y < surface->height; i++){
        for(int j = 0; j < rect.size.x && j + rect.pos.x < surface->width; j++){
            buffer[(i + rect.pos.y) * (surface->width + surface->linePadding / 4 /*Padding should always be divisible by 4*/) + (j + rect.pos.x)] = colour_i;
        // Memset here has been causing issues // memset32_optimized((void*)(buffer + (i + rect.pos.y) * (surface->width + surface->linePadding / 4) + rect.pos.x), colour_i, ((rect.pos.x + rect.size.x) < surface->width) ? rect.size.x : (surface->width - rect.pos.x));
        }
    }
}

video_mode_t GetVideoMode(){
    video_mode_t v;
    //syscall(SYS_GETVIDEOMODE, (uint32_t)&v, 0, 0, 0, 0);
    return v;
}

void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, surface_t* surface){
    if(x < 0){
        width += x;
        x = 0;
    }

    if(y < 0){
        height += y;
        y = 0;
    }
    
    uint32_t colour_i = (r << 16) | (g << 8) | b;
    uint64_t colour_l = (colour_i << 32) | colour_i;
    uint32_t* buffer = (uint32_t*)surface->buffer; // Convert byte array into an array of 32-bit unsigned integers as the supported colour depth is 32 bit
    for(int i = 0; i < height && (i + y) < surface->height; i++){
        uint32_t yOffset = (i + y) * (surface->width + surface->linePadding / 4 /*Padding should always be divisible by 4*/);
        /*for(int j = 0; j < width && (x + j) < surface->width; j++){
            //buffer[(i + y) * surface->width + (j + x)] = colour_i;
            /*if(!(j % sizeof(uint64_t))){
                ((uint64_t*)(buffer + ((i + y) * surface->width + x) * 4))[j/sizeof(uint64_t)] = colour_l;
                j += 1;
            } else* /
            {
                buffer[yOffset + (j + x)] = colour_i;
            }*/
        //}
            
        memset32_optimized((void*)(buffer + yOffset + x), colour_i, ((x + width) < surface->width) ? width : (surface->width - x));
    }
}

void DrawRect(int x, int y, int width, int height, rgba_colour_t colour, surface_t* surface){
    DrawRect(x,y,width,height,colour.r,colour.g,colour.b,surface);
}

void DrawBitmapImage(int x, int y, int w, int h, uint8_t *data, surface_t* surface) {
    bitmap_file_header_t bmpHeader = *(bitmap_file_header_t*)data;
    data += bmpHeader.offset;

	uint8_t bmpBpp = 24;
    uint32_t rowSize = floor((bmpBpp*w + 31) / 32) * 4;
	uint32_t bmp_offset = rowSize * (h - 1);
	uint32_t bmp_buffer_offset = 0;

	uint32_t pixelSize = 4;
	for (int i = 0; i < h && i + y < surface->height; i++) {
		for (int j = 0; j < w && j + x < surface->width; j++) {
            if(data[bmp_offset + j * (bmpBpp / 8)] == 1 && data[bmp_offset + j * (bmpBpp / 8) + 1] == 1 && data[bmp_offset + j * (bmpBpp / 8) + 2] == 1) continue;
			uint32_t offset = (y + i)*(surface->width*pixelSize + surface->linePadding) + (x + j) * pixelSize;
			surface->buffer[offset] = data[bmp_offset + j * (bmpBpp / 8)];
			surface->buffer[offset + 1] = data[bmp_offset + j * (bmpBpp / 8) + 1];
			surface->buffer[offset + 2] = data[bmp_offset + j * (bmpBpp / 8) + 2];
		}
		bmp_offset -= rowSize;
		bmp_buffer_offset += w * pixelSize;
	}
}

void DrawGradient(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface){
    if(x < 0){
        width += x;
        x = 0;
    }

    if(y < 0){
        height += y;
        y = 0;
    }
    
    uint32_t colour_i = 0;//(colour.r << 16) | (colour.g << 8) | colour.b;
    uint32_t* buffer = (uint32_t*)surface->buffer; // Convert byte array into an array of 32-bit unsigned integers as the supported colour depth is 32 bit
    for(int j = 0; j < width && (x + j) < surface->width; j++){
            DrawRect(x + j, y, 1, height, (uint8_t)(j*(((double)c2.r - (double)c1.r)/width)+c1.r),(uint8_t)(j*(((double)c2.g - (double)c1.g)/width)+c1.g),(uint8_t)(j*(((double)c2.b - (double)c1.b)/width)+c1.b),surface);
    }
}

void DrawGradientVertical(int x, int y, int width, int height, rgba_colour_t c1, rgba_colour_t c2, surface_t* surface){
    if(x < 0){
        width += x;
        x = 0;
    }

    if(y < 0){
        height += y;
        y = 0;
    }
    
    uint32_t* buffer = (uint32_t*)surface->buffer; // Convert byte array into an array of 32-bit unsigned integers as the supported colour depth is 32 bit
    for(int j = 0; j < height && (y + j) < surface->height; j++){
            DrawRect(x, y + j, width, 1, (uint8_t)(j*(((double)c2.r - (double)c1.r)/height)+c1.r),(uint8_t)(j*(((double)c2.g - (double)c1.g)/height)+c1.g),(uint8_t)(j*(((double)c2.b - (double)c1.b)/height)+c1.b),surface);
    }
}

void surfacecpy(surface_t* dest, surface_t* src, vector2i_t offset){
	uint32_t* srcBuffer = (uint32_t*)src->buffer;
	uint32_t* destBuffer = (uint32_t*)dest->buffer;
	for(int i = 0; i < src->height && i < dest->height - offset.y; i++){
		/*for(int j = 0; j < src->width && j < dest->width - offset.x; j++){
			destBuffer[(i+offset.y)*dest->width + j + offset.x] = srcBuffer[i*src->width + j];
		}*/
        //memcpy_optimized(dest->buffer + ((i+offset.y)*(dest->width*4 + dest->linePadding) + offset.x*4), src->buffer + i*src->width*4, src->width*4);
	}
}

void surfacecpyTransparent(surface_t* dest, surface_t* src, vector2i_t offset){
	uint32_t* srcBuffer = (uint32_t*)src->buffer;
	uint32_t* destBuffer = (uint32_t*)dest->buffer;
	for(int i = 0; i < src->height && i < dest->height - offset.y; i++){
		for(int j = 0; j < src->width && j < dest->width - offset.x; j++){
			if((srcBuffer[i*src->width + j] >> 24) < 255) continue;
			destBuffer[(i+offset.y)*dest->width + j + offset.x] = srcBuffer[i*src->width + j];
		}
	}
}
