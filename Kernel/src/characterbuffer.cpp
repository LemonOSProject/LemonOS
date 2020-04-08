#include <characterbuffer.h>

#include <string.h>
#include <liballoc.h>
#include <logging.h>

CharacterBuffer::CharacterBuffer(){
    buffer = (char*)kmalloc(bufferSize);
}

size_t CharacterBuffer::Write(char* _buffer, size_t size){
    if((bufferPos + size) > bufferSize) {
        krealloc(buffer, bufferPos + size + 32);
        bufferSize = bufferPos + size + 32;
    }

    memcpy((buffer + bufferPos), _buffer, size);
    bufferPos += size;

    Log::Warning("buffer pos:");
    Log::Info(bufferPos, false);

    return size;
}

size_t CharacterBuffer::Read(char* _buffer, size_t count){
    if(count > bufferPos) count = bufferPos;
 
    if(count <= 0) return 0;

    memcpy(_buffer, buffer, count);

    memcpy(buffer, buffer + count, bufferSize - count);

    bufferPos -= count;

    Log::Warning("buffer pos:");
    Log::Info(bufferPos, false);

    return count;
}