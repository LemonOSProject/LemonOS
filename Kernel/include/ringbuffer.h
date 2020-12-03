#pragma once

#include <stddef.h>
#include <memory.h>
#include <spin.h>

template<typename T>
class RingBuffer{
private:
    T* buffer = nullptr;
    T* bufferEnd = nullptr;
    size_t bufferSize = 0;

    T* enqueuePointer = nullptr;
    T* dequeuePointer = nullptr;

    lock_t enqueueLock = 0;
    lock_t dequeueLock = 0;
public:
    RingBuffer(){
        bufferSize = 64;
        buffer = reinterpret_cast<T*>(kmalloc(sizeof(T) * bufferSize));

        bufferEnd = &buffer[bufferSize];

        enqueuePointer = buffer;
        dequeuePointer = buffer;
    }

    void Enqueue(T* data){
        acquireLock(&dequeueLock);

        memcpy(data, enqueuePointer++, sizeof(T));
        
        if(enqueuePointer >= bufferEnd){
            enqueuePointer = buffer;
        }

        if(enqueuePointer == dequeuePointer){

            T* oldBuffer = buffer;
            T* oldBufferEnd = bufferEnd;

            bufferSize = (bufferSize + 2) << 1;

            acquireLock(&enqueueLock);
            buffer = reinterpret_cast<T*>(kmalloc(sizeof(T) * bufferSize));
            bufferEnd = &buffer[bufferSize - 1];
            
            memcpy(bufferEnd - (oldBufferEnd - dequeuePointer), dequeuePointer, oldBufferEnd - dequeuePointer);

            dequeuePointer = bufferEnd - (oldBufferEnd - dequeuePointer);
            enqueuePointer = buffer + (enqueuePointer - oldBuffer);
            releaseLock(&enqueueLock);

            kfree(oldBuffer);
        }
        releaseLock(&dequeueLock);
    }

    int Dequeue(T* data, size_t amount){
        acquireLock(&dequeueLock);
        if(dequeuePointer == enqueuePointer){
            releaseLock(&dequeueLock);
            return 0;
        }

        size_t counter = 0;
        while(dequeuePointer != enqueuePointer && counter < amount) {
            memcpy(data++, dequeuePointer++, sizeof(T));
            counter++;

            if(dequeuePointer >= bufferEnd){
                dequeuePointer = buffer;
            }
        }
        releaseLock(&dequeueLock);

        return counter;
    }
};