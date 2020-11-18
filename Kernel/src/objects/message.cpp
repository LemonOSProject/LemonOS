#include <objects/message.h>

MessageEndpoint::MessageEndpoint(uint16_t maxSize){
    maxMessageSize = maxSize;

    if(maxMessageSize > maxMessageSizeLimit){
        maxMessageSize = maxMessageSizeLimit;
    }

    messageQueueLimit = 0x800000 / maxMessageSize; // No more than 8MB
    messageCacheLimit = 0x100000 / maxMessageSize; // No more than 1MB

    queueAvailablilitySemaphore.SetValue(messageQueueLimit);
}

int64_t MessageEndpoint::Read(uint64_t* id, uint16_t* size, uint64_t* data){
    assert(id);
    assert(size);
    assert(data);

    acquireLock(&queueLock);
    unsigned queueLen = queue.get_length();

    if(!queueLen){
        releaseLock(&queueLock);
        return 1;
    }

    Message* m = queue.remove_at(0);
    queueAvailablilitySemaphore.Signal();

    releaseLock(&queueLock);

    *id = m->id;
    *size = m->size;
    *data = m->data;

    acquireLock(&cacheLock);
    if(cache.get_length() < messageCacheLimit){
        cache.add_back(m);
    }
    releaseLock(&cacheLock);

    return 0;
}

int64_t MessageEndpoint::Call(uint64_t id, uint16_t size, uint64_t data, uint64_t rID, uint16_t* rSize, uint64_t* rData, uint64_t timeout){
    assert(rSize);
    assert(rData);
    
    return -1;
}

int64_t MessageEndpoint::Write(uint64_t id, uint16_t size, uint64_t data){
    return -1;
}