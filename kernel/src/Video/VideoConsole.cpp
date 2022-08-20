
#include <Video/VideoConsole.h>

#include <MM/KMalloc.h>
#include <Math.h>
#include <String.h>
#include <Video/Video.h>

VideoConsole::VideoConsole(int x, int y, int width, int height) {
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;

    cursorX = 0;
    cursorY = 0;

    widthInCharacters = width / 8 - 1;
    heightInCharacters = height / 8 - 1;

    characterBuffer =
        (ConsoleCharacter*)kmalloc(widthInCharacters * (heightInCharacters + 1) *
                                   sizeof(ConsoleCharacter)); // One ConsoleCharacter is 4 bytes (char, r, g, b)
    memset(characterBuffer, 0, widthInCharacters * heightInCharacters * sizeof(ConsoleCharacter));
}

void VideoConsole::Update() {
    Video::DrawRect(x, y, width, height, 32, 32, 32);
    for (int i = 0; i < heightInCharacters; i++) {
        for (int j = 0; j < widthInCharacters; j++) {
            ConsoleCharacter c = characterBuffer[i * widthInCharacters + j];
            if (c.c == 0)
                continue;
            Video::DrawChar(c.c, x + j * 8, y + i * 8, c.r, c.g, c.b);
        }
    }
}

void VideoConsole::Print(char c, uint8_t r, uint8_t g, uint8_t b) {
    switch (c) {
    case '\n':
        cursorX = 0;
        cursorY++;
        break;
    case '\b':
        cursorX--;
        if (cursorX < 0) {
            cursorX = widthInCharacters - 1;
            cursorY--;
            if (cursorY < 0) {
                cursorX = cursorY = 0;
            }
        }
        break;
    default:
        characterBuffer[cursorY * widthInCharacters + cursorX] = {c, r, g, b};
        cursorX++;
        break;
    }
    if (cursorX >= widthInCharacters) {
        cursorX = 0;
        cursorY++;
    }
    if (cursorY >= heightInCharacters)
        Scroll();
}

void VideoConsole::Clear(uint8_t r, uint8_t g, uint8_t b) {
    memset(characterBuffer, 0, widthInCharacters * heightInCharacters * sizeof(ConsoleCharacter));
    cursorX = 0;
    cursorY = 0;
}

void VideoConsole::Print(const char* str, uint8_t r, uint8_t g, uint8_t b) {
    while (*str != '\0') {
        Print(*str++, r, g, b);
    }
}

void VideoConsole::PrintN(const char* str, unsigned n, uint8_t r, uint8_t g, uint8_t b) {
    unsigned i = 0;
    while (*str && i < n) {
        Print(*str++, r, g, b);
        i++;
    }
}

void VideoConsole::Scroll() {
    memcpy(characterBuffer, (void*)(characterBuffer + widthInCharacters),
           widthInCharacters * (heightInCharacters - 1) * sizeof(ConsoleCharacter));
    cursorY--;
    memset(characterBuffer + cursorY * widthInCharacters, 0, widthInCharacters * sizeof(ConsoleCharacter));
}