#pragma once

#include <stdint.h>

struct ConsoleCharacter
{
    char c;
    uint8_t r, g, b;
} __attribute__((packed));

class VideoConsole{
    public:
    int cursorX;
    int cursorY;

    int x;
    int y;
    int width;
    int height;

    int widthInCharacters;
    int heightInCharacters;

    ConsoleCharacter* characterBuffer = nullptr;
    uint32_t* videoBuffer = nullptr;
    
    VideoConsole(int x, int y, int width, int height);

    void Reposition(int x, int y, int width, int height);

    void Update();
    void Clear(uint8_t r, uint8_t g, uint8_t b);

    void Print(char c, uint8_t r, uint8_t g, uint8_t b);
    void Print(const char* str, uint8_t r, uint8_t g, uint8_t b);
    void PrintN(const char* str, unsigned n, uint8_t r, uint8_t g, uint8_t b);

    private:
    void Scroll();
};