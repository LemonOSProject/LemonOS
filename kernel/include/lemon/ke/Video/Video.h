#include <stdint.h>

#include <MM/VMObject.h>
#include <MiscHdr.h>
#include <RefPtr.h>

namespace Video {

void Initialize(video_mode_t videoMode);
video_mode_t GetVideoMode();
FancyRefPtr<VMObject> GetFramebufferVMO();

void DrawRect(void* buffer, unsigned int x, unsigned int y, unsigned int width, unsigned int height, uint8_t r,
              uint8_t g, uint8_t b);
void DrawChar(void* buffer, char c, unsigned int x, unsigned int y, uint8_t r, uint8_t g, uint8_t b, int vscale = 1,
              int hscale = 1);
void DrawString(void* buffer, const char* str, unsigned int x, unsigned int y, uint8_t r, uint8_t g, uint8_t b,
                int vscale = 1, int hscale = 1);

void DrawBitmapImage(void* buffer, unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint8_t* data);

} // namespace Video
