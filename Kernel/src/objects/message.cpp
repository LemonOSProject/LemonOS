#include <objects/message.h>

#include <errno.h>

MessageEndpoint::MessageEndpoint(uint16_t maxSize){
    maxMessageSize = maxSize;

    if(maxMessageSize > maxMessageSizeLimit){
        maxMessageSize = maxMessageSizeLimit;
    }

    messageQueueLimit = 0x500000 / maxMessageSize; // No more than 5MB
    messageCacheLimit = 0x80000 / maxMessageSize; // No more than 512KB

    queueAvailablilitySemaphore.SetValue(messageQueueLimit);
}

MessageEndpoint::~MessageEndpoint(){
    while(cache.get_length()){
        delete cache.remove_at(0);
    }

    while(bufferCache.get_length()){
        delete[] bufferCache.remove_at(0);
    }

    while(queue.get_length()){
        auto* m = queue.remove_at(0);

        if(m->size > 8){
            delete[] m->dataP;
        }

        delete m;
    }
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

    if(m->size <= 8){
        *data = m->data;
    } else {
        memcpy(data, m->dataP, m->size);
    }

    if(m->size > 8){
        acquireLock(&bufferCacheLock);
        if(bufferCache.get_length() < messageCacheLimit){
            bufferCache.add_back(m->dataP);
            releaseLock(&cacheLock);
        } else {
            releaseLock(&bufferCacheLock);
            delete[] m->dataP;
        }
    }

    acquireLock(&cacheLock);
    if(cache.get_length() < messageCacheLimit){
        cache.add_back(m);
        releaseLock(&cacheLock);
    } else {
        releaseLock(&cacheLock);
        delete m;
    }

    return 0;
}

int64_t MessageEndpoint::Call(uint64_t id, uint16_t size, uint64_t data, uint64_t rID, uint16_t* rSize, uint64_t* rData, uint64_t timeout){
    assert(rSize);
    assert(rData);
    
    return -1;
}

int64_t MessageEndpoint::Write(uint64_t id, uint16_t size, uint64_t data){
    assert(peer.get());

    if(size > maxMessageSize){
        return -EINVAL;
    }

    Message* msg;

    acquireLock(&peer->cacheLock);
    if(peer->cache.get_length() > 0){
        msg = peer->cache.remove_at(0);
        releaseLock(&peer->cacheLock);
    } else {
        releaseLock(&peer->cacheLock);

        msg = new Message;
    }

    msg->id = id;
    msg->size = size;
    
    if(msg->size <= 8){
        msg->data = data;
    } else {
        assert(data);

        acquireLock(&peer->bufferCacheLock);
        if(peer->cache.get_length() > 0){
            msg->dataP = peer->bufferCache.remove_at(0);
            releaseLock(&peer->bufferCacheLock);
        } else {
            releaseLock(&peer->bufferCacheLock);
            msg->dataP = new uint8_t[maxMessageSize];
        }

        memcpy(msg->dataP, reinterpret_cast<uint8_t*>(data), size);
    }

    acquireLock(&peer->queueLock);
    peer->queue.add_back(msg);
    releaseLock(&peer->queueLock);
    return 0;
}