#pragma once

#define CHARBUFFER_START_SIZE 1024

#include <stddef.h>

class CharacterBuffer{
public:
    size_t bufferSize = CHARBUFFER_START_SIZE;
    size_t bufferPos = 0;
    int lines;
    char* buffer;
    bool ignoreBackspace = false;

    CharacterBuffer();

    size_t Write(char* buffer, size_t size);
    size_t Read(char* buffer, size_t count);

private:
    int lock = 0;
};