#pragma once

#include <stdint.h>

#include <list.h>
#include <refptr.h>
#include <pair.h>
#include <lock.h>
#include <ringbuffer.h>

#include <objects/kobject.h>

struct Message{
    uint64_t id;
    union{
        uint64_t data; // Empty, pointer or integer
        uint8_t* dataP;
    };
    uint16_t size;

    Message* next;
    Message* prev;
};

class MessageEndpoint final : public KernelObject{
private:
    friend Pair<FancyRefPtr<MessageEndpoint>,FancyRefPtr<MessageEndpoint>> CreatePair();
    uint16_t maxMessageSize = 8;
    uint16_t messageQueueLimit = 128;
    lock_t queueLock = 0;

    Semaphore queueAvailablilitySemaphore = Semaphore(messageQueueLimit);

    RingBuffer<Message> queue;

    FancyRefPtr<MessageEndpoint> peer;
public:
    static const uint16_t maxMessageSizeLimit = UINT16_MAX;
    
    static Pair<FancyRefPtr<MessageEndpoint>,FancyRefPtr<MessageEndpoint>> CreatePair(uint16_t msgSize){
        FancyRefPtr<MessageEndpoint> endpoint1 = FancyRefPtr<MessageEndpoint>(new MessageEndpoint(msgSize));
        FancyRefPtr<MessageEndpoint> endpoint2 = FancyRefPtr<MessageEndpoint>(new MessageEndpoint(msgSize));

        endpoint1->peer = endpoint2;
        endpoint2->peer = endpoint1;

        return {endpoint1, endpoint2};
    }

    MessageEndpoint(uint16_t maxSize);
    ~MessageEndpoint();

    /////////////////////////////
    /// \brief Read a message from the queue
    ///
    /// Check if there are any available messages for reading and attempts to read one off the queue.
    ///
    /// \param id Pointer to the ID to be populated
    /// \param size Pointer to the message size to be populated
    /// \param data Pointer to an unsigned integer representing either 8 bytes of data (size <= 8) or a pointer to a buffer of size length containing message data
    ///
    /// \return 0 on success, 1 on empty, negative error code on failure
    /////////////////////////////
    int64_t Read(uint64_t* id, uint16_t* size, uint64_t* data);
    
    /////////////////////////////
    /// \brief Send a message and return the response
    ///
    /// Sends a message to the peer and blocks until it receives a reply or the timeout period is expired.
    /// Useful for RPC
    ///
    /// \param id ID of the message to be sent
    /// \param size Size of the message to be sent
    /// \param data Either a pointer to data (size > 8) or 8 bytes of data to be sent (size <= 8)
    ///
    /// \param rID ID of the expected message
    /// \param size Pointer to the message size to be populated
    /// \param data Pointer to an unsigned integer representing either 8 bytes of data (size <= 8) or a pointer to a buffer of size length containing message data
    ///
    /// \param timeout Amount of time (in us) until timeout
    ///
    /// \return 0 on success, 1 on timeout, negative error code on failure
    /////////////////////////////
    int64_t Call(uint64_t id, uint16_t size, uint64_t data, uint64_t rID, uint16_t* rSize, uint64_t* rData, uint64_t timeout);
    
    
    /////////////////////////////
    /// \brief Send a message
    ///
    /// Sends a message to the peer
    ///
    /// \param id ID of the message to be sent
    /// \param size Size of the message to be sent
    /// \param data Either a pointer to data (size > 8) or 8 bytes of data to be sent (size <= 8)
    ///
    /// \return 0 on success, negative error code on failure
    /////////////////////////////
    int64_t Write(uint64_t id, uint16_t size, uint64_t data);

    uint16_t GetMaxMessageSize() const { return maxMessageSize; }

    inline static constexpr kobject_id_t TypeID() { return KOBJECT_ID_MESSAGE_ENDPOINT; }
    kobject_id_t InstanceTypeID() const { return TypeID(); }
};