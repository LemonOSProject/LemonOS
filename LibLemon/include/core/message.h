#pragma once

#include <stdint.h>
#include <vector>
#include <deque>
#include <memory>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string>
#include <string.h>

#define LEMON_MESSAGE_MAGIC 0xBEEF

#define LEMON_MESSAGE_PROTOCOL_WMEVENT 1
#define LEMON_MESSAGE_PROTOCOL_WMCMD 2
#define LEMON_MESSAGE_PROTOCOL_SHELLCMD 3

#include <stddef.h>

namespace Lemon{
    struct LemonMessage{
        unsigned short magic = LEMON_MESSAGE_MAGIC;
        unsigned short length = 0;
        unsigned int protocol = 0;
        uint8_t data[0];
    };
    
    using MessageRawDataObject = std::pair<uint8_t*, uint16_t>; // length, data

    class Message{
        friend class MessageIterator;
    private:
        LemonMessage header;
        uint8_t* mdata;

        template<typename T>
        uint16_t GetSize(T& obj){
            return sizeof(obj);
        }

        template<typename T>
        void Insert(uint16_t& pos, T& obj){
            memcpy(&mdata[pos], &obj, sizeof(T));
            pos += sizeof(T);
        }
    public:
        template<typename T>
        static MessageRawDataObject EncodeGenericData(const T* data, uint16_t length){
            return MessageRawDataObject((uint8_t*)data, length);
        }

        static MessageRawDataObject EncodeString(const std::string& str){
            return EncodeGenericData<char>(str.data(), str.length());
        }

        template<typename ...T>
        Message(unsigned int protocol, T... objects){
            header.length = (GetSize(objects) + ... + header.length); // Get total length of objects
            header.magic = LEMON_MESSAGE_MAGIC;
            header.protocol = protocol;

            mdata = new uint8_t[sizeof(LemonMessage) + header.length];

            uint16_t pos = 0;
            Insert<LemonMessage>(pos, header);
            (void)std::initializer_list<int>{ ((void)Insert(pos, objects), 0)... }; // HACK: Call insert for each object
        }

        Message(uint8_t* data){
            mdata = data;
        }

        const uint8_t* data() const { return mdata; }

        uint16_t length() const { return sizeof(LemonMessage) + header.length; }

        ~Message(){
            delete mdata;
        }
    };

    class MessageIterator {
        friend class Message;
    protected:
        Message* m;
        size_t off = 0;
    public:
        MessageIterator(Message* m){
            this->m = m;
        }

        template<typename T>
        void Consume(T& ref){
            memcpy(&ref, m->mdata, m->GetSize(ref));
            off += m->GetSize(ref);
        }
    };

    template<>
    uint16_t Message::GetSize<MessageRawDataObject>(MessageRawDataObject& obj);

    template<>
    void Message::Insert<MessageRawDataObject>(uint16_t& pos, MessageRawDataObject& obj);
}