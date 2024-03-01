#include "video.h"

#include <string.h>

namespace video {

uint32_t *framebuffer = nullptr;
uint32_t screen_width;
uint32_t screen_height;

void set_mode(void* fb, uint32_t _width, uint32_t _height) {
    framebuffer = (uint32_t*)fb;
    
    screen_width = _width;
    screen_height = _height;
}

uint64_t get_screen_width() {
    return screen_width;
}

uint64_t get_screen_height() {
    return screen_height;
}


void fill_rect(int x, int y, int width, int height, uint32_t color) {
    if (!framebuffer)
        return;

    if (y < 0)
        y = 0;
    if (x < 0)
        x = 0;
    if (y > screen_height)
        return;
    if (x > screen_width)
        return;

    if (y + height > screen_height)
        height = screen_height - y;
    if (x + width > screen_width)
        width = screen_width - x;

    uint32_t* line_start = framebuffer + (y * screen_width + x);
    while (height--) {
        int count = width;

        uint32_t *ptr = line_start;
        while(count--) {
            *(ptr++) = color;
        }

        line_start += screen_width;
    }
}

void draw_string(const char* str, int x, int y, uint32_t color) {
    if (!framebuffer)
        return;
}

} // namespace video
