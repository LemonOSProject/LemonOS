#include <core/message.h>

#include <string.h>

namespace Lemon{
    template<>
    void Message::Insert<MessageRawDataObject>(uint16_t& pos, MessageRawDataObject& obj){
        memcpy(&mdata[pos], obj.first, obj.second);
        pos += obj.second;
    }

    template<>
    uint16_t Message::GetSize<MessageRawDataObject>(MessageRawDataObject& obj){
        return obj.second;
    }
}