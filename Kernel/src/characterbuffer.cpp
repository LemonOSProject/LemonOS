#include <characterbuffer.h>

#include <string.h>
#include <liballoc.h>
#include <logging.h>

CharacterBuffer::CharacterBuffer(){
    buffer = (char*)kmalloc(bufferSize);
    bufferPos = 0;
}

size_t CharacterBuffer::Write(char* _buffer, size_t size){
    if((bufferPos + size) > bufferSize) {
        bufferSize = bufferPos + size + 32;
        krealloc(buffer, bufferSize);
    }

    for(int i = 0; i < size; i++){
        buffer[bufferPos + i] = _buffer[i];
    }

    bufferPos += size;

    return size;
}

size_t CharacterBuffer::Read(char* _buffer, size_t count){
    if(count > bufferPos) {
        count = bufferPos;
    }
 
    if(count <= 0) return 0;

    for(int i = 0; i < count; i++){
        _buffer[i] = buffer[i];
    }

    for(int i = 0; i < bufferSize - count; i++){
        buffer[i] = buffer[count + i];
    }

    bufferPos -= count;

    return count;
}