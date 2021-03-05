#include <Objects/Interface.h>

#include <String.h>
#include <Scheduler.h>

MessageInterface::MessageInterface(const char* _name, uint16_t msgSize){
    name = strdup(_name);

    if(msgSize > MessageEndpoint::maxMessageSizeLimit){
        msgSize = MessageEndpoint::maxMessageSizeLimit;
    }

    this->msgSize = msgSize;
}

MessageInterface::~MessageInterface(){
    Destroy();
}

void MessageInterface::Destroy(){
    active = false;

    for(auto& i : incoming){
        i->item1 = -1;
    }
}

long MessageInterface::Accept(FancyRefPtr<MessageEndpoint>& endpoint){
    acquireLock(&incomingLock);
    if(incoming.get_length() > 0){
        auto connection = incoming.remove_at(0);
        releaseLock(&incomingLock);

        auto channel = MessageEndpoint::CreatePair(msgSize);
        connection->item2 = channel.item2;
        connection->item1 = true;

        endpoint = channel.item1; 
        return 1;
    } else {
        releaseLock(&incomingLock);
        return 0;
    }
}

FancyRefPtr<MessageEndpoint> MessageInterface::Connect(){
    if(!active){
        return FancyRefPtr<MessageEndpoint>();
    }

    Pair<int, FancyRefPtr<MessageEndpoint>> connection;

    connection.item1 = false;

    acquireLock(&incomingLock);
    incoming.add_back(&connection);
    releaseLock(&incomingLock);

    acquireLock(&waitingLock);
    while(waiting.get_length() > 0){
        waiting.remove_at(0)->Signal();
    }
    releaseLock(&waitingLock);

    while(!connection.item1){
        Scheduler::Yield();
    }

    return connection.item2;
}