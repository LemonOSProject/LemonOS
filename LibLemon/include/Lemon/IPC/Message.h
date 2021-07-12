#pragma once

#include <deque>
#include <memory>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

namespace Lemon {
enum CoreMessageIDs {
    MessagePeerDisconnect = 0,
    MessageObjectRequest = 1,
    MessageObjectResponse = 2,
};

using MessageRawDataObject = std::pair<uint8_t*, uint16_t>; // length, data

class Message final {
    friend class Interface;
    friend class Endpoint;

  public:
    template <typename T> static uint16_t GetSize(const T& obj) { return sizeof(obj); }

    template <typename... T> inline static uint16_t GetSize(const T&... objects) { return (GetSize(objects) + ...); }

    enum {
        OK,
        ErrorDecodeOutOfBounds,
        ErrorBufferNotInitialized,
    };

    template <typename T> static MessageRawDataObject EncodeGenericData(const T* data, uint16_t length) {
        return MessageRawDataObject((uint8_t*)data, sizeof(T) * length);
    }

    Message() : m_size(0), m_data(nullptr) {}

    Message(uint64_t id) : m_id(id), m_size(0), m_data(nullptr) {}

    template <typename... T> Message(uint64_t id, const T&... objects) : m_id(id) {
        m_size = (GetSize(objects) + ...); // Get total length of objects

        if (m_size > 0) {
            m_data = new uint8_t[m_size];
        }

        uint16_t pos = 0;
        (void)std::initializer_list<int>{((void)Insert(pos, objects), 0)...}; // HACK: Call insert for each object
    }

    template <typename... T>
    Message(uint8_t* data, uint16_t size, uint64_t id, const T&... objects)
        : m_id(id), m_size(size), m_data(data) { // Don't calculate the size as the caller should know the size
                                                 // beforehand as they have created a buffer
        uint16_t pos = 0;
        (void)std::initializer_list<int>{((void)Insert(pos, objects), 0)...}; // HACK: Call insert for each object
    }

    Message(uint8_t* data, uint16_t size, uint64_t id) : m_id(id), m_size(size), m_data(data) {}

    explicit Message(const Message& m) : m_id(m.m_id), m_size(m.m_size), m_data(new uint8_t[m.m_size]) {
        memcpy(m_data, m.m_data, m_size);
    }

    Message& operator=(Message&& m) noexcept {
        m_id = m.m_id;
        m_size = m.m_size;
        m_data = m.m_data;

        m.m_size = 0;
        m.m_data = nullptr;

        return *this;
    }

    void Set(uint8_t* data, uint16_t size, uint64_t id) {
        m_id = id;
        m_size = size;
        m_data = data;
    }

    template <typename... T> long Decode(T&... objects) const {
        if (!m_data) {
            return ErrorBufferNotInitialized;
        }

        uint16_t pos = 0;
        if (long ret = Decode(pos, objects...); ret) {
            return ret;
        }

        return 0;
    }

    inline const uint8_t* data() const { return m_data; }
    inline uint16_t length() const { return m_size; }
    inline uint64_t id() const { return m_id; }

    ~Message() {
        if (m_data) {
            delete[] m_data;
        }
    }

  private:
    uint64_t m_id;
    uint16_t m_size;
    uint8_t* m_data;

    Message& operator=(const Message& m);

    template <typename T> void Insert(uint16_t& pos, const T& obj) {
        memcpy(&m_data[pos], &obj, sizeof(T));
        pos += sizeof(T);
    }

    template <typename O> long Decode(uint16_t& pos, O& o) const {
        uint8_t* currentData = m_data + pos;

        if (pos + sizeof(O) > m_size) {
            return ErrorDecodeOutOfBounds;
        }

        o = *(O*)currentData;
        pos += sizeof(O);

        return 0;
    }

    template <typename O, typename... T> long Decode(uint16_t& pos, O& object, T&... objects) const {
        if (long ret = Decode(pos, object); ret) {
            return ret;
        }

        if (long ret = Decode(pos, objects...); ret) {
            return ret;
        }

        return 0;
    }
};

template <> uint16_t Message::GetSize<MessageRawDataObject>(const MessageRawDataObject& obj);

template <> uint16_t Message::GetSize<std::string>(const std::string& obj);

template <> void Message::Insert<MessageRawDataObject>(uint16_t& pos, const MessageRawDataObject& obj);

template <> void Message::Insert<std::string>(uint16_t& pos, const std::string& obj);

template <> long Message::Decode<MessageRawDataObject>(uint16_t& pos, MessageRawDataObject& obj) const;

template <> long Message::Decode<std::string>(uint16_t& pos, std::string& obj) const;
} // namespace Lemon
