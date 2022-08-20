#include <Lemon/Graphics/Graphics.h>

#include <cmath>

namespace Lemon::Graphics {
Texture::Texture(vector2i_t size) : size(size) {
    surface = {.width = size.x, .height = size.y, .depth = 32, .buffer = new uint8_t[size.x * size.y * 4]};
}

Texture::~Texture() {
    if (source.buffer) {
        delete[] source.buffer;
    }

    delete[] surface.buffer;
}

void Texture::SetSize(vector2i_t newSize) {
    if (newSize == size) {
        return; // If the new size is equal to the old size don't do anything
    }

    size = newSize;

    delete[] surface.buffer; // Create new surface
    surface = {.width = size.x, .height = size.y, .depth = 32, .buffer = new uint8_t[size.x * size.y * 4]};

    UpdateSurface();
}

void Texture::Blit(vector2i_t pos, surface_t* dest) { surfacecpy(dest, &surface, pos); }

void Texture::LoadSourcePixels(surface_t* sourcePixels) {
    if (source.buffer) {
        delete[] source.buffer;
    }

    source = *sourcePixels;
    source.buffer = new uint8_t[source.width * source.height * 4];

    memcpy(source.buffer, sourcePixels->buffer, source.width * source.height * 4);

    UpdateSurface();
}

void Texture::AdoptSourcePixels(surface_t* sourcePixels) {
    if (source.buffer) {
        delete[] source.buffer;
    }

    source = *sourcePixels;

    UpdateSurface();
}

void Texture::UpdateSurface() {
    if (!source.buffer) {
        return; // No source pixels
    }

    if (scaling == ScaleNone) {
        surfacecpy(&surface, &source); // No scaling
    } else {
        double xScale = ((double)size.x) / source.width;
        double yScale = ((double)size.y) / source.height;

        if (scaling == ScaleFit) {
            if (yScale < xScale)
                xScale = yScale;
            else
                yScale = xScale;
        }

        uint8_t* srcBuffer = source.buffer;
        for (int i = 0; i < surface.height; i++) {
            double _yval = ((double)i) / yScale;
            if (ceil(_yval) >= source.height)
                break;
            for (int j = 0; j < surface.width; j++) {
                double _xval = ((double)j) / xScale;
                if (ceil(_xval) >= source.width)
                    break;
                uint32_t offset = i * surface.width + j;

                long b = Interpolate(srcBuffer[((int)floor(_yval) * source.width + (int)floor(_xval)) * 4],
                                     srcBuffer[((int)floor(_yval) * source.width + (int)ceil(_xval)) * 4],
                                     srcBuffer[((int)ceil(_yval) * source.width + (int)floor(_xval)) * 4],
                                     srcBuffer[((int)ceil(_yval) * source.width + (int)ceil(_xval)) * 4], _xval, _yval);
                long g =
                    Interpolate(srcBuffer[((int)floor(_yval) * source.width + (int)floor(_xval)) * 4 + 1],
                                srcBuffer[((int)floor(_yval) * source.width + (int)ceil(_xval)) * 4 + 1],
                                srcBuffer[((int)ceil(_yval) * source.width + (int)floor(_xval)) * 4 + 1],
                                srcBuffer[((int)ceil(_yval) * source.width + (int)ceil(_xval)) * 4 + 1], _xval, _yval);
                long r =
                    Interpolate(srcBuffer[((int)floor(_yval) * source.width + (int)floor(_xval)) * 4 + 2],
                                srcBuffer[((int)floor(_yval) * source.width + (int)ceil(_xval)) * 4 + 2],
                                srcBuffer[((int)ceil(_yval) * source.width + (int)floor(_xval)) * 4 + 2],
                                srcBuffer[((int)ceil(_yval) * source.width + (int)ceil(_xval)) * 4 + 2], _xval, _yval);
                long a =
                    Interpolate(srcBuffer[((int)floor(_yval) * source.width + (int)floor(_xval)) * 4 + 3],
                                srcBuffer[((int)floor(_yval) * source.width + (int)ceil(_xval)) * 4 + 3],
                                srcBuffer[((int)ceil(_yval) * source.width + (int)floor(_xval)) * 4 + 3],
                                srcBuffer[((int)ceil(_yval) * source.width + (int)ceil(_xval)) * 4 + 3], _xval, _yval);

                reinterpret_cast<uint32_t*>(surface.buffer)[offset] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}
} // namespace Lemon::Graphics
