#pragma once

#include <stdint.h>

#include <list.h>
#include <refptr.h>
#include <pair.h>

#include <objects/kobject.h>

struct Message{
    enum{
        TypeShortMessage, // Instead of pointing to memory, the message consists of one 64-bit integer
        TypeMemoryMessage, // The message contains more data
    };

    uint64_t id;
    uint32_t type;
    uint32_t size;
    uint64_t data; // Empty, pointer or integer

    Message* next;
};

class MessageEndpoint : public KernelObject{
private:
    friend Pair<FancyRefPtr<MessageEndpoint>,FancyRefPtr<MessageEndpoint>> CreatePair();

    FastList<Message*> queue;

    FancyRefPtr<MessageEndpoint> peer;
public:
    static Pair<FancyRefPtr<MessageEndpoint>,FancyRefPtr<MessageEndpoint>> CreatePair(){
        FancyRefPtr<MessageEndpoint> endpoint1 = FancyRefPtr<MessageEndpoint>();
        FancyRefPtr<MessageEndpoint> endpoint2 = FancyRefPtr<MessageEndpoint>();

        endpoint1->peer = endpoint2;
        endpoint2->peer = endpoint1;

        return {endpoint1, endpoint2};
    }

    MessageEndpoint();

    int64_t Read();

    int64_t Write();
};

using MessageChannel = Pair<MessageEndpoint, MessageEndpoint>;