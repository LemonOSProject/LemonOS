#include <objects/message.h>

#include <errno.h>
#include <logging.h>

MessageEndpoint::MessageEndpoint(uint16_t maxSize){
    maxMessageSize = maxSize;

    if(maxMessageSize > maxMessageSizeLimit){
        maxMessageSize = maxMessageSizeLimit;
    }

    messageQueueLimit = 0x300000 / maxMessageSize; // No more than 3MB

    queueAvailablilitySemaphore.SetValue(messageQueueLimit);

    if(debugLevelMessageEndpoint >= DebugLevelNormal){
        Log::Info("[MessageEndpoint] new endpoint with message size of %u (Queue limit: %u)", maxMessageSize, messageQueueLimit);
    }
}

MessageEndpoint::~MessageEndpoint(){
    Message m;
    while(queue.Dequeue(&m, 1)){
        if(m.size > 8){
            delete[] m.dataP;
        }
    }

    if(peer.get()){
        peer->peer = nullptr;
    }
}

int64_t MessageEndpoint::Read(uint64_t* id, uint16_t* size, uint64_t* data){
    assert(id);
    assert(size);
    assert(data);
    if(debugLevelMessageEndpoint >= DebugLevelVerbose){
        //Log::Info("[MessageEndpoint] %u messages in queue!", queueLen);
    }

    Message m;
    
    int r = queue.Dequeue(&m, 1);
    if(r < 1){
        return 0;
    }

    queueAvailablilitySemaphore.Signal();

    *id = m.id;
    *size = m.size;

    if(m.size <= 8){
        *data = m.data;
    } else {
        memcpy(data, m.dataP, m.size);
    }

    if(m.size > 8){
        delete[] m.dataP;
    }

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

    Message msg;

    msg.id = id;
    msg.size = size;
    
    if(msg.size <= 8){
        msg.data = data;
    } else {
        assert(data);

        msg.dataP = new uint8_t[maxMessageSize];

        memcpy(msg.dataP, reinterpret_cast<uint8_t*>(data), size);
    }


    acquireLock(&peer->queueLock);
    //peer->queue.add_back(msg);
    peer->queue.Enqueue(&msg);

    if(debugLevelMessageEndpoint >= DebugLevelVerbose){
        Log::Info("[MessageEndpoint] Sending message (ID: %u, Size: %u) to peer", id, size);
        //Log::Info("[MessageEndpoint] %u messages in peer queue", peer->queue.get_length());
    }

    releaseLock(&peer->queueLock);
    return 0;
}