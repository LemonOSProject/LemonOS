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
    Destroy();
}

void MessageEndpoint::Destroy(){
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

    Message m;
    
    int r = queue.Dequeue(&m, 1);
    if(r < 1){
        if(!peer.get()){
            return -ENOTCONN;
        }
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

    if(debugLevelMessageEndpoint >= DebugLevelVerbose){
        Log::Info("[MessageEndpoint] Receiving message (ID: %u, Size: %u)", m.id, m.size);
    }

    if(m.size > 8){
        delete[] m.dataP;
    }

    return 1;
}

int64_t MessageEndpoint::Call(uint64_t id, uint16_t size, uint64_t data, uint64_t rID, uint16_t* rSize, uint64_t* rData, int64_t timeout){
    if(!peer.get()){
        return -ENOTCONN;
    }

    if(size > maxMessageSize){
        return -EINVAL;
    }

    assert(rSize);
    assert(rData);
    
    Semaphore s = Semaphore(0);
    Message m;

    acquireLock(&waitingResponseLock);
    waitingResponse.add_back({&s, {&m, rID}});
    releaseLock(&waitingResponseLock);

    Write(id, size, data); // Send message

    // TODO: timeout
    s.Wait(); // Await reponse

    *rSize = m.size;

    if(m.size <= 8){
        *rData = m.data;
    } else {
        memcpy(rData, m.dataP, m.size);
    }

    if(m.size > 8){
        delete[] m.dataP;
    }

    return 0;
}

int64_t MessageEndpoint::Write(uint64_t id, uint16_t size, uint64_t data){
    if(!peer.get()){
        return -ENOTCONN;
    }

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

    acquireLock(&peer->waitingResponseLock);
    for(auto it = peer->waitingResponse.begin(); it != peer->waitingResponse.end(); it++){
        if(it->item2.id == id){
            it->item1->Signal();
            *it->item2.ret = msg; 

            if(debugLevelMessageEndpoint >= DebugLevelVerbose){
                Log::Info("[MessageEndpoint] Sending response (ID: %u, Size: %u) to peer", msg.id, msg.size);
            }

            peer->waitingResponse.remove(it);
            releaseLock(&peer->waitingResponseLock);
            return 0; // Skip queue entirely
        }
    }
    releaseLock(&peer->waitingResponseLock);

    acquireLock(&peer->queueLock);
    peer->queue.Enqueue(&msg);

    acquireLock(&peer->waitingLock);
    while(peer->waiting.get_length() > 0){
        peer->waiting.remove_at(0)->Signal();
    }
    releaseLock(&peer->waitingLock);

    if(debugLevelMessageEndpoint >= DebugLevelVerbose){
        Log::Info("[MessageEndpoint] Sending message (ID: %u, Size: %u) to peer", msg.id, msg.size);
    }

    releaseLock(&peer->queueLock);
    return 0;
}