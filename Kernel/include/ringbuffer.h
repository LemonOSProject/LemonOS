#pragma once

#include <stddef.h>
#include <memory.h>
#include <spin.h>

template<typename T = uint8_t>
class RingBuffer{
public:
    RingBuffer(){
        bufferSize = 64;
        buffer = reinterpret_cast<T*>(kmalloc(sizeof(T) * bufferSize));

        bufferEnd = &buffer[bufferSize];

        enqueuePointer = buffer;
        dequeuePointer = buffer;
    }

    RingBuffer(unsigned bSize) : bufferSize(bSize) {
        buffer = reinterpret_cast<T*>(kmalloc(sizeof(T) * bufferSize));

        bufferEnd = &buffer[bufferSize];

        enqueuePointer = buffer;
        dequeuePointer = buffer;
    }

    void Enqueue(const T* data){
        acquireLock(&enqueueLock);
        acquireLock(&dequeueLock);

        memcpy(enqueuePointer++, data, sizeof(T));
        
        if(enqueuePointer >= bufferEnd){
            enqueuePointer = buffer;
        }

        if(enqueuePointer == dequeuePointer){
            Resize((bufferSize + 2) << 1);
        }

        releaseLock(&enqueueLock);
        releaseLock(&dequeueLock);
    }

    void EnqueueUnlocked(const T* data, size_t count){
        size_t contiguousCount = count;
        size_t wrappedCount = 0;

        if(enqueuePointer < dequeuePointer && enqueuePointer + count > dequeuePointer){
            Resize((bufferSize << 1) + count); // Ring buffer is full
        } else if(enqueuePointer + contiguousCount > bufferEnd){
            contiguousCount = (uintptr_t)(bufferEnd - enqueuePointer); // Get number of contiguous elements to copy
            wrappedCount = count - contiguousCount; // Get number of elements that did not fit

            if(buffer + wrappedCount >= dequeuePointer){
                Resize((bufferSize << 1) + count); // Ring buffer is full
            }
        } 

        memcpy(enqueuePointer, data, sizeof(T) * contiguousCount);
        
        if(wrappedCount > 0){
            memcpy(buffer, data + contiguousCount, sizeof(T) * wrappedCount);
        }

        size += count;
        enqueuePointer = buffer + ((enqueuePointer + count - buffer) % bufferSize); // Calculate new enqueuePointer
    }

    inline void Enqueue(const T* data, size_t count){
        acquireLock(&enqueueLock);
        acquireLock(&dequeueLock);

        EnqueueUnlocked(data, count);

        releaseLock(&enqueueLock);
        releaseLock(&dequeueLock);
    }

    int Dequeue(T* data, size_t amount){
        if(dequeuePointer == enqueuePointer){
            return 0;
        }

        acquireLock(&dequeueLock);

        size_t contiguous = amount;
        size_t wrapped = 0;
        if(dequeuePointer < enqueuePointer && dequeuePointer + amount > enqueuePointer){
            contiguous = enqueuePointer - dequeuePointer;
        } else if(dequeuePointer + contiguous > bufferEnd){
            contiguous = bufferEnd - dequeuePointer;
            wrapped = amount - contiguous;
            
            if(buffer + wrapped > enqueuePointer){
                wrapped = enqueuePointer - buffer;
            }
        }

        memcpy(data, dequeuePointer, contiguous * sizeof(T));
        dequeuePointer += contiguous;

        if(dequeuePointer >= bufferEnd){
            dequeuePointer = buffer + wrapped;

            if(wrapped){
                memcpy(data + contiguous, buffer, wrapped * sizeof(T));
            }
        }

        size -= contiguous + wrapped;
        releaseLock(&dequeueLock);

        return contiguous + wrapped;
    }

    void Drain() {
        acquireLock(&enqueueLock);
        acquireLock(&dequeueLock);

        enqueuePointer = dequeuePointer = buffer;
        size = 0;

        releaseLock(&enqueueLock);
        releaseLock(&dequeueLock);
    }

    void Drain(size_t count) {
        acquireLock(&dequeueLock);

        if(dequeuePointer <= enqueuePointer && dequeuePointer + count > enqueuePointer){
            size = 0;
            dequeuePointer = enqueuePointer;
        } else if(dequeuePointer + count > bufferEnd){
            if(buffer + count > enqueuePointer){
                size = 0;
                dequeuePointer = enqueuePointer;
            } else {
                dequeuePointer = buffer + count;
                size -= count;
            }
        } else {
            size -= count;
            dequeuePointer += count;
        }

        releaseLock(&dequeueLock);
    }

    inline unsigned Count() { return size; }
    inline bool Empty() { return dequeuePointer == enqueuePointer; }
protected:
    void Resize(size_t size){
        T* oldBuffer = buffer;
        T* oldBufferEnd = bufferEnd;

        bufferSize = size;

        buffer = reinterpret_cast<T*>(kmalloc(sizeof(T) * bufferSize));
        bufferEnd = &buffer[bufferSize];
        
        memcpy(bufferEnd - (oldBufferEnd - dequeuePointer), dequeuePointer, (oldBufferEnd - dequeuePointer) * sizeof(T));
        if(dequeuePointer > enqueuePointer){
            memcpy(buffer, oldBuffer, (enqueuePointer - oldBuffer) * sizeof(T));
        }

        dequeuePointer = bufferEnd - (oldBufferEnd - dequeuePointer);
        enqueuePointer = buffer + (enqueuePointer - oldBuffer);

        kfree(oldBuffer);
    }

    T* buffer = nullptr;
    T* bufferEnd = nullptr;
    size_t bufferSize = 0;

    T* enqueuePointer = nullptr;
    T* dequeuePointer = nullptr;

    unsigned size;

    lock_t enqueueLock = 0;
    lock_t dequeueLock = 0;
};

class RawRingBuffer : public RingBuffer<uint8_t>{
public:
    template<typename ...T>
    inline constexpr void EnqueueObjects(const T&... data){
        acquireLock(&enqueueLock);
        acquireLock(&dequeueLock);

        EnqueueUnlocked(data...);

        releaseLock(&enqueueLock);
        releaseLock(&dequeueLock);
    }

private:
    template<typename T, typename I>
    inline constexpr void EnqueueUnlocked(const Pair<T*, I>& data){
        RingBuffer::EnqueueUnlocked(data.item1, data.item2 * sizeof(T));
    }

    template<typename O, typename ...T>
    inline constexpr void EnqueueUnlocked(const O& obj, const T&... data){
        EnqueueUnlocked<O>(obj);
        
        EnqueueUnlocked(data...);
    }

    template<typename T>
    inline constexpr void EnqueueUnlocked(const T& data){
        RingBuffer::EnqueueUnlocked(reinterpret_cast<uint8_t const*>(&data), sizeof(T));
    }
};