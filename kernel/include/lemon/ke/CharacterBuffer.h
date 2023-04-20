#pragma once

#define CHARBUFFER_START_SIZE 1024

#include <stddef.h>
#include <Types.h>

#include <Error.h>
#include <UserIO.h>

class CharacterBuffer{
public:
    size_t bufferSize = CHARBUFFER_START_SIZE;
    size_t maxBufferSize = 2097152;
    size_t bufferPos = 0;
    int lines = 0;
    bool ignoreBackspace = false;

    CharacterBuffer();
    ~CharacterBuffer();

    ssize_t write(char* buffer, size_t size);
    ssize_t read(char* buffer, size_t count);
    ErrorOr<ssize_t> write(UIOBuffer* buffer, size_t size);
    ErrorOr<ssize_t> read(UIOBuffer* buffer, size_t count);

    void flush();
private:
    char* m_buffer = nullptr;
    
    volatile int lock = 0;
};