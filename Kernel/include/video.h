#include <mischdr.h>
#include <stdint.h>

namespace Video{
    void Initialize(video_mode_t videoMode);
    video_mode_t GetVideoMode();

    void DrawRect(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b);
    void DrawChar(char c, unsigned int x, unsigned int y, uint8_t r, uint8_t g, uint8_t b);
    void DrawString(char* str, unsigned int x, unsigned int y, uint8_t r, uint8_t g, uint8_t b);

    void DrawBitmapImage(int x, int y, int w, int h, uint8_t *data);
}