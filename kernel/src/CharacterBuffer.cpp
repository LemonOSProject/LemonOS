#include <CharacterBuffer.h>

#include <CString.h>
#include <Lock.h>
#include <Logging.h>
#include <MM/KMalloc.h>

CharacterBuffer::CharacterBuffer() {
    m_buffer = new char[bufferSize];
    bufferPos = 0;
    lock = 0;
    lines = 0;
}

CharacterBuffer::~CharacterBuffer() {
    if(m_buffer){
        delete m_buffer;
    }
}

ssize_t CharacterBuffer::write(char* _buffer, size_t size) {
    if (size == 0) {
        return 0;
    }

    ScopedSpinLock lock{this->lock};
    if ((bufferPos + size) > bufferSize) {
        size = bufferSize - bufferPos;
        if (bufferPos + size > maxBufferSize) {
            size = maxBufferSize - bufferPos;
        }

        char* oldBuf = m_buffer;
        m_buffer = new char[bufferPos + size + 128];
        memcpy(m_buffer, oldBuf, bufferPos);
        bufferSize = bufferPos + size + 128;

        if(oldBuf) {
            delete oldBuf;
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
            m_buffer[bufferPos++] = _buffer[i];
            written++;
        }

        if (_buffer[i] == '\n' || _buffer[i] == '\0')
            lines++;
    }

    return written;
}

ssize_t CharacterBuffer::read(char* _buffer, size_t count) {
    ScopedSpinLock lock{this->lock};
    if (count <= 0) {
        return 0;
    }

    int i = 0;
    int readPos = 0;
    for (; readPos < bufferPos && i < count; readPos++) {
        if (m_buffer[readPos] == '\0') {
            lines--;
            continue;
        }

        _buffer[i] = m_buffer[readPos];
        i++;

        if (m_buffer[readPos] == '\n')
            lines--;
    }

    for (unsigned j = 0; j < bufferSize - readPos; j++) {
        m_buffer[j] = m_buffer[readPos + j];
    }

    bufferPos -= readPos;

    return i;
}

void CharacterBuffer::flush() {
    ScopedSpinLock lock{this->lock};

    bufferPos = 0;
    lines = 0;
}