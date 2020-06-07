#include <characterbuffer.h>

#include <string.h>
#include <liballoc.h>
#include <logging.h>
#include <lock.h>

CharacterBuffer::CharacterBuffer(){
    buffer = (char*)kmalloc(bufferSize);
    bufferPos = 0;
    lock = 0;
    lines = 0;
}

size_t CharacterBuffer::Write(char* _buffer, size_t size){
    acquireLock(&(this->lock));

    if((bufferPos + size) > bufferSize) {
        char* oldBuf = buffer;
        buffer = (char*)kmalloc(bufferPos + size + 128);
        memcpy(buffer, oldBuf, bufferSize);
        bufferSize = bufferPos + size + 128;
        kfree(oldBuf);
    }

    size_t written = 0;

    for(int i = 0; i < size; i++){
        if(_buffer[i] == '\b' /*Backspace*/ && !ignoreBackspace){
            if(bufferPos > 0) {
                bufferPos--;
                written++;
            }
            continue;
        } else {
            buffer[bufferPos++] = _buffer[i];
            written++;
        }

        if(_buffer[i] == '\n' || _buffer[i] == '\0') lines++;
    }

    releaseLock(&(this->lock));

    return written;
}

size_t CharacterBuffer::Read(char* _buffer, size_t count){
    acquireLock(&(this->lock));

    if(count > bufferPos) {
        count = bufferPos;
    }
 
    if(count <= 0) {
        releaseLock(&(this->lock));
        return 0;
    }

    for(int i = 0; i < count; i++){
        if(buffer[i] == '\0') {
            lines--;
            continue;
        }

        _buffer[i] = buffer[i];

        if(buffer[i] == '\n') lines--;
    }

    for(int i = 0; i < bufferSize - count; i++){
        buffer[i] = buffer[count + i];
    }

    bufferPos -= count;

    releaseLock(&(this->lock));

    return count;
}

void CharacterBuffer::Flush(){
    acquireLock(&(this->lock));

    bufferPos = 0;
    lines = 0;
    
    releaseLock(&(this->lock));
}