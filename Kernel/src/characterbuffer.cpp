#include <characterbuffer.h>

#include <string.h>
#include <liballoc.h>
#include <logging.h>
#include <lock.h>

CharacterBuffer::CharacterBuffer(){
    buffer = (char*)kmalloc(bufferSize);
    bufferPos = 0;
    lock = 0;
}

size_t CharacterBuffer::Write(char* _buffer, size_t size){
    acquireLock(&lock);

    if((bufferPos + size) > bufferSize) {
        bufferSize = bufferPos + size + 32;
        krealloc(buffer, bufferSize);
    }

    for(int i = 0; i < size; i++){
        if(_buffer[i] == '\b' /*Backspace*/ && !ignoreBackspace){
            if(bufferPos > 0) bufferPos--;
        } else {
            buffer[bufferPos++] = _buffer[i];
        }

        if(_buffer[i] == '\n') lines++;
    }

    //bufferPos += size;

    releaseLock(&lock);

    return size;
}

size_t CharacterBuffer::Read(char* _buffer, size_t count){
    acquireLock(&lock);

    if(count > bufferPos) {
        count = bufferPos;
    }
 
    if(count <= 0) {
        releaseLock(&lock);
        return 0;
    }

    for(int i = 0; i < count; i++){
        _buffer[i] = buffer[i];

        if(buffer[i] == '\n') lines--;
    }

    for(int i = 0; i < bufferSize - count; i++){
        buffer[i] = buffer[count + i];
    }

    bufferPos -= count;

    releaseLock(&lock);

    return count;
}