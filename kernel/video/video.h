#pragma once

#include <stdint.h>

namespace video {

void set_mode(void *fb, uint32_t width, uint32_t height);
uint64_t get_screen_width();
uint64_t get_screen_height();

void fill_rect(int x, int y, int width, int height, uint32_t color);
void draw_string(const char *str, int x, int y, uint32_t color);

} // namespace video
