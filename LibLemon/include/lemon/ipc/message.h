#pragma once

#include <stdint.h>
#include <vector>
#include <deque>
#include <memory>
#include <poll.h>
#include <string>
#include <string.h>
#include <stddef.h>

namespace Lemon{
    enum CoreMessageIDs{
        MessagePeerDisconnect = 0,
        MessageObjectRequest = 1,
        MessageObjectResponse = 2,
    };

    using MessageRawDataObject = std::pair<uint8_t*, uint16_t>; // length, data

    class Message final {
    protected:
        friend class Interface;
        friend class Endpoint;
        uint8_t* mdata;
    private:
        uint16_t msize;
        uint64_t mid;

        Message& operator=(const Message& m);

        template<typename T>
        uint16_t GetSize(const T& obj) const {
            return sizeof(obj);
        }

        template<typename T>
        void Insert(uint16_t& pos, const T& obj){
            memcpy(&mdata[pos], &obj, sizeof(T));
            pos += sizeof(T);
        }

        template<typename O>
        long Decode(uint16_t& pos, O& o){
            uint8_t* currentData = mdata + pos;

            if(pos + sizeof(O) > msize){
                return ErrorDecodeOutOfBounds;
            }

            o = *(O*)currentData;
            pos += sizeof(O);

            return 0;
        }

        template<typename O, typename ...T>
        long Decode(uint16_t& pos, O& object, T&... objects){
            if(long ret = Decode(pos, object); ret){
                return ret;
            }

            if(long ret = Decode(pos, objects...); ret){
                return ret;
            }

            return 0;
        }
    public:
        enum {
            OK,
            ErrorDecodeOutOfBounds,
            ErrorBufferNotInitialized,
        };

        template<typename T>
        static MessageRawDataObject EncodeGenericData(const T* data, uint16_t length){
            return MessageRawDataObject((uint8_t*)data, sizeof(T) * length);
        }

        Message(){
            msize = 0;
            mdata = nullptr;
            mid = 0;
        }

        Message(uint64_t id){
            mid = id;
            msize = 0;
            mdata = nullptr;
        }

        template<typename ...T>
        Message(uint64_t id, const T&... objects){
            msize = (GetSize(objects) + ...); // Get total length of objects
            mid = id;

            if(msize > 0){
                mdata = new uint8_t[msize];
            }

            uint16_t pos = 0;
            (void)std::initializer_list<int>{ ((void)Insert(pos, objects), 0)... }; // HACK: Call insert for each object
        }

        //Message(uint8_t* data, uint16_t size, uint64_t id) : mdata(data), msize(size), mid(id) {}

        explicit Message(const Message& m) {
            msize = m.msize;
            mid = m.mid;

            mdata = new uint8_t[msize];
            memcpy(mdata, m.mdata, msize);
        }
        
        Message& operator=(Message&& m) noexcept {
            mdata = m.mdata;
            mid = m.mid;
            msize = m.msize;

            return *this;
        }

        void Set(uint8_t* data, uint16_t size, uint64_t id) {
            mdata = data;
            msize = size;
            mid = id;
        }
        
        template<typename ...T>
        long Decode(T&... objects){
            if(!mdata){
                return ErrorBufferNotInitialized;
            }

            uint16_t pos = 0;
            if(long ret = Decode(pos, objects...); ret){
                return ret;
            }

            return 0;
        }

        inline const uint8_t* data() const { return mdata; }
        inline uint16_t length() const { return msize; }
        inline uint64_t id() const {return mid; }

        ~Message(){
            if(msize && mdata)
                delete mdata;
        }
    };

    template<>
    uint16_t Message::GetSize<MessageRawDataObject>(const MessageRawDataObject& obj) const;
    
    template<>
    uint16_t Message::GetSize<std::string>(const std::string& obj) const;

    template<>
    void Message::Insert<MessageRawDataObject>(uint16_t& pos, const MessageRawDataObject& obj);

    template<>
    void Message::Insert<std::string>(uint16_t& pos, const std::string& obj);

    template<>
    long Message::Decode<MessageRawDataObject>(uint16_t& pos, MessageRawDataObject& obj);

    template<>
    long Message::Decode<std::string>(uint16_t& pos, std::string& obj);
}