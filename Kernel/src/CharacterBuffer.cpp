#include <CharacterBuffer.h>

#include <CString.h>
#include <Lock.h>
#include <Logging.h>
#include <MM/KMalloc.h>

CharacterBuffer::CharacterBuffer() {
    buffer = new char[bufferSize];
    bufferPos = 0;
    lock = 0;
    lines = 0;
}

CharacterBuffer::~CharacterBuffer() {
    if(buffer){
        delete buffer;
    }
}

ssize_t CharacterBuffer::Write(char* _buffer, size_t size) {
    if (bufferPos + size > maxBufferSize) {
        size = maxBufferSize - bufferPos;
    }

    if (size == 0) {
        return 0;
    }

    ScopedSpinLock lock{this->lock};

    if ((bufferPos + size) > bufferSize) {
        char* oldBuf = buffer;
        buffer = (char*)kmalloc(bufferPos + size + 128);
        memcpy(buffer, oldBuf, bufferSize);
        bufferSize = bufferPos + size + 128;

        if(oldBuf) {
            kfree(oldBuf);
        }
    }

    ssize_t written = 0;

    for (unsigned i = 0; i < size; i++) {
        if (_buffer[i] == '\b' /*Backspace*/ && !ignoreBackspace) {
            if (bufferPos > 0) {
                bufferPos--;
                written++;
            }
            continue;
        } else {
            buffer[bufferPos++] = _buffer[i];
            written++;
        }

        if (_buffer[i] == '\n' || _buffer[i] == '\0')
            lines++;
    }

    return written;
}

ssize_t CharacterBuffer::Read(char* _buffer, size_t count) {
    ScopedSpinLock lock{this->lock};
    if (count <= 0) {
        return 0;
    }

    int i = 0;
    int readPos = 0;
    for (; readPos < bufferPos && i < count; readPos++) {
        if (buffer[readPos] == '\0') {
            lines--;
            continue;
        }

        _buffer[i] = buffer[readPos];
        i++;

        if (buffer[readPos] == '\n')
            lines--;
    }

    for (unsigned j = 0; j < bufferSize - readPos; j++) {
        buffer[j] = buffer[readPos + j];
    }

    bufferPos -= readPos;

    return i;
}

void CharacterBuffer::Flush() {
    ScopedSpinLock lock{this->lock};

    bufferPos = 0;
    lines = 0;
}