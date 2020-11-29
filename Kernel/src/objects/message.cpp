#include <objects/message.h>

#include <errno.h>
#include <logging.h>

MessageEndpoint::MessageEndpoint(uint16_t maxSize){
    maxMessageSize = maxSize;

    if(maxMessageSize > maxMessageSizeLimit){
        maxMessageSize = maxMessageSizeLimit;
    }

    messageQueueLimit = 0x300000 / maxMessageSize; // No more than 3MB
    messageCacheLimit = 0x80000 / maxMessageSize; // No more than 512KB

    queueAvailablilitySemaphore.SetValue(messageQueueLimit);

    if(debugLevelMessageEndpoint >= DebugLevelNormal){
        Log::Info("[MessageEndpoint] new endpoint with message size of %u (Queue limit: %u, Cache limit: %u)", maxMessageSize, messageQueueLimit, messageCacheLimit);
    }
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

    if(peer.get()){
        peer->peer = nullptr;
    }
}

int64_t MessageEndpoint::Read(uint64_t* id, uint16_t* size, uint64_t* data){
    assert(id);
    assert(size);
    assert(data);

    acquireLock(&queueLock);
    unsigned queueLen = queue.get_length();

    if(queueLen <= 0){
        releaseLock(&queueLock);
        return 0;
    }

    if(debugLevelMessageEndpoint >= DebugLevelVerbose){
        Log::Info("[MessageEndpoint] %u messages in queue!", queueLen);
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
        /*acquireLock(&bufferCacheLock);
        if(bufferCache.get_length() < messageCacheLimit){
            bufferCache.add_back(m->dataP);
            releaseLock(&cacheLock);
        } else {
            releaseLock(&bufferCacheLock);*/
            delete[] m->dataP;
        //}
    }

    /*acquireLock(&cacheLock);
    if(cache.get_length() < messageCacheLimit){
        cache.add_back(m);
        releaseLock(&cacheLock);
    } else {
        releaseLock(&cacheLock);*/
        delete m;
    //}

    return 1;
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

    /*peer->queueAvailablilitySemaphore.Wait();

    acquireLock(&peer->cacheLock);
    if(peer->cache.get_length() > 0){
        msg = peer->cache.remove_at(0);
        releaseLock(&peer->cacheLock);
    } else {
        releaseLock(&peer->cacheLock);*/

        msg = new Message;
    //}

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

    if(debugLevelMessageEndpoint >= DebugLevelVerbose){
        Log::Info("[MessageEndpoint] Sending message (ID: %u, Size: %u) to peer", id, size);
        Log::Info("[MessageEndpoint] %u messages in peer queue", peer->queue.get_length());
    }

    releaseLock(&peer->queueLock);
    return 0;
}