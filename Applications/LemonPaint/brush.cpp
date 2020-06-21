#include "paint.h"

#include <math.h>
#include <stdio.h>

void Brush::Paint(int x, int y, uint8_t r, uint8_t g, uint8_t b, double scale, Canvas* canvas){
    int _x = x - (data.width / 2 * scale);
    int _y = y - (data.height / 2 * scale);
    for(int i = 0; i < data.height * scale; i++){
        if(_y + i < 0 || _y + i >= canvas->surface.height) continue;
        for(int j = 0; j < data.width * scale; j++){
            if(_x + j < 0 || _x + j >= canvas->surface.width) continue;

            double _xval = ((double)j) / scale;
            double _yval = ((double)i) / scale;

            double val1 = data.buffer[(int)floor(_yval) * data.width + (int)floor(_xval)];
            double val2 = data.buffer[(int)floor(_yval) * data.width + (int)ceil(_xval)];
            double x1 = (val1 + (val2 - val1) * (_xval - ((int)_xval)));

            val1 = data.buffer[(int)ceil(_yval) * data.width + (int)floor(_xval)];
            val2 = data.buffer[(int)ceil(_yval) * data.width + (int)ceil(_xval)];
            double x2 = (val1 + (val2 - val1) * (_xval - ((int)_xval)));

            double val = (x1 + (x2 - x1) * (_yval - ((int)_yval))) / 255.0;

            uint32_t oldColour = ((uint32_t*)canvas->surface.buffer)[(_y + i) * canvas->surface.width + _x + j];
            double oldB = oldColour & 0xFF;
            double oldG = (oldColour >> 8) & 0xFF;
            double oldR = (oldColour >> 16) & 0xFF;

            uint32_t newColour = (uint32_t)(val * b + oldB * (1 - val)) | (((uint32_t)(val * g + oldG * (1 - val)) << 8)) | (((uint32_t)(val * r + oldR * (1 - val)) << 16));
            
            ((uint32_t*)canvas->surface.buffer)[(_y + i) * canvas->surface.width +_x + j] = newColour;
        }
    }
}