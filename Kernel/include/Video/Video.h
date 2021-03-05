#include <MiscHdr.h>
#include <stdint.h>

namespace Video{
    void Initialize(video_mode_t videoMode);
    video_mode_t GetVideoMode();

    void DrawRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, uint8_t r, uint8_t g, uint8_t b);
    void DrawChar(char c, unsigned int x, unsigned int y, uint8_t r, uint8_t g, uint8_t b);
    void DrawString(const char* str, unsigned int x, unsigned int y, uint8_t r, uint8_t g, uint8_t b);

    void DrawBitmapImage(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t *data);
}