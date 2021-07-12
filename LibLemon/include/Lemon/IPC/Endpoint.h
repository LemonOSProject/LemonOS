#pragma once

#include <Lemon/System/IPC.h>
#include <Lemon/System/KernelObject.h>
#include <Lemon/System/Waitable.h>
#include <Lemon/Types.h>

#include <Lemon/IPC/Message.h>

#include <assert.h>
#include <stdint.h>

namespace Lemon {
class EndpointException : public std::exception {
  public:
    enum {
        EndpointUnknownError,
        EndpointInterfacePathResolutionError,
        EndpointInvalidHandle,
        EndpointNotConnected,
    };

  protected:
    int error = 0;
    static const char* const errorStrings[];

  public:
    EndpointException(int error) { this->error = error; }

    const char* what() const noexcept { return errorStrings[error]; }
};

class Endpoint : public Waitable {
  public:
    Endpoint() = default;

    Endpoint(const Lemon::Endpoint& other) = delete;

    Endpoint(Lemon::Endpoint&& other) : handle(other.handle), msgSize(other.msgSize) {
        assert(handle > 0);

        other.handle = 0;
    }

    Endpoint(handle_t h, uint16_t msgSize) {
        assert(h > 0);

        handle = h;
        this->msgSize = msgSize;
    }

    Endpoint(handle_t h) {
        assert(h > 0);

        handle = h;

        LemonEndpointInfo endpInfo;
        if (long ret = EndpointInfo(h, endpInfo); ret) {
            if (ret == -EINVAL) {
                printf("[LibLemon] Invalid endpoint handle!\n");
                throw new EndpointException(EndpointException::EndpointInvalidHandle);
            } else {
                printf("[LibLemon] Endpoint error %ld\n!", ret);
                throw new EndpointException(EndpointException::EndpointUnknownError);
            }
        }
        this->msgSize = endpInfo.msgSize;
    }

    Endpoint(const std::string& path) : Endpoint(path.c_str()) {}

    Endpoint(const char* path) {
        handle = InterfaceConnect(path);

        if (handle <= 0) {
            printf("[LibLemon] Error %ld connecting to interface %s!\n", handle, path);
            throw new EndpointException(EndpointException::EndpointInterfacePathResolutionError);
        }

        LemonEndpointInfo endpInfo;
        if (long ret = EndpointInfo(handle, endpInfo); ret) {
            if (ret == -EINVAL) {
                throw new EndpointException(EndpointException::EndpointInvalidHandle);
            } else {
                throw new EndpointException(EndpointException::EndpointUnknownError);
            }
        }
        this->msgSize = endpInfo.msgSize;
    }

    Lemon::Endpoint& operator=(Lemon::Endpoint&& other) {
        handle = other.handle;
        msgSize = other.msgSize;

        assert(handle);

        other.handle = 0;

        return *this;
    }

    ~Endpoint() { DestroyKObject(handle); }

    /////////////////////////////
    /// \brief Close Endpoint
    ///
    /// Destroy the handle to the endpoint,
    /// the actual endpoint will be destroyed when any peers destroy their handles
    /////////////////////////////
    inline void Close() {
        DestroyKObject(handle);

        handle = 0;
    }

    inline operator handle_t() { return handle; }

    /////////////////////////////
    /// \brief Get the Kernel Handle to the Endpoint
    ///
    /// Returns a Lemon OS handle ID pointing to the endpoint.
    ///
    /// \return Handle ID of endpoint
    /////////////////////////////
    inline handle_t GetHandle() { return handle; }

    /////////////////////////////
    /// \brief Get the message size
    ///
    /// \return Maximum message size
    /////////////////////////////
    inline uint16_t GetMessageSize() const { return msgSize; }

    inline long Queue(uint64_t id, const uint8_t* data, uint16_t size) {
        return EndpointQueue(handle, id, size, reinterpret_cast<uintptr_t>(data));
    }

    inline long Queue(uint64_t id, uint64_t data, uint16_t size) { return EndpointQueue(handle, id, size, data); }

    inline long Queue(const Message& m) {
        return EndpointQueue(handle, m.id(), m.length(), reinterpret_cast<uintptr_t>(m.data()));
    }

    inline long Poll(Message& m) {
        uint64_t id;
        uint16_t size;
        uint8_t* data = new uint8_t[msgSize];
        long ret = EndpointDequeue(handle, &id, &size, data);

        if (ret > 0) {
            m.Set(data, size, id);
        } else {
            delete[] data;
        }
        return ret;
    }

    inline long Call(const Message& call, Message& rmsg, uint64_t id) {
        uint16_t size = call.length();
        uint8_t* data = new uint8_t[msgSize];

        long ret = EndpointCall(handle, call.id(), reinterpret_cast<uintptr_t>(call.data()), id,
                                reinterpret_cast<uintptr_t>(data), &size);

        if (!ret) {
            rmsg.Set(data, size, id);
        } else {
            delete[] data;
        }
        return ret;
    }

    inline long Call(Message& call, uint64_t id) { // Use the same buffer for return
        uint16_t size = call.length();
        uint8_t* data = const_cast<uint8_t*>(call.data());

        long ret = EndpointCall(handle, call.id(), reinterpret_cast<uintptr_t>(call.data()), id,
                                reinterpret_cast<uintptr_t>(call.data()), &size);

        if (!ret) {
            call.Set(data, size, id);
        }
        return ret;
    }

  protected:
    handle_t handle = 0;
    uint16_t msgSize = 512;
};
}; // namespace Lemon
