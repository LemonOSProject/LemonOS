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

    ssize_t Write(char* buffer, size_t size);
    ssize_t Read(char* buffer, size_t count);
    ErrorOr<ssize_t> Write(UIOBuffer* buffer, size_t size);
    ErrorOr<ssize_t> Read(UIOBuffer* buffer, size_t count);

    void Flush();
private:
    char* m_buffer = nullptr;
    
    volatile int lock = 0;
};