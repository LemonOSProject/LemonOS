#include <core/message.h>

#include <string.h>

namespace Lemon{
    template<>
    void Message::Insert<MessageRawDataObject>(uint16_t& pos, MessageRawDataObject& obj){
        memcpy(&mdata[pos], &obj.second, sizeof(unsigned short));
        memcpy(&mdata[pos + sizeof(unsigned short)], obj.first, obj.second);
        pos += obj.second;
    }

    template<>
    uint16_t Message::GetSize<MessageRawDataObject>(MessageRawDataObject& obj){
        return sizeof(unsigned short) + obj.second; // 2 byte length + data
    }
}