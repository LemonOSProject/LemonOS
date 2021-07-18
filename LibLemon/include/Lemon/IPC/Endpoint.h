#pragma once

#include <Lemon/System/Handle.h>
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

    Endpoint(Lemon::Endpoint&& other) : m_handle(std::move(other.m_handle)), m_msgSize(other.m_msgSize) {
        assert(m_handle.get() > 0);

        other.m_handle = Handle();
    }

    Endpoint(Handle h, uint16_t msgSize) {
        assert(h.get() > 0);

        m_handle = h;
        this->m_msgSize = msgSize;
    }

    Endpoint(Handle h) {
        assert(h.get() > 0);

        m_handle = h;

        LemonEndpointInfo endpInfo;
        if (long ret = EndpointInfo(m_handle.get(), endpInfo); ret) {
            if (ret == -EINVAL) {
                printf("[LibLemon] Invalid endpoint handle!\n");
                throw new EndpointException(EndpointException::EndpointInvalidHandle);
            } else {
                printf("[LibLemon] Endpoint error %ld\n!", ret);
                throw new EndpointException(EndpointException::EndpointUnknownError);
            }
        }
        this->m_msgSize = endpInfo.msgSize;
    }

    Endpoint(const std::string& path) : Endpoint(path.c_str()) {}

    Endpoint(const char* path) {
        handle_t handle = InterfaceConnect(path);

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
        m_handle = Handle(handle);
        m_msgSize = endpInfo.msgSize;
    }

    Lemon::Endpoint& operator=(Lemon::Endpoint&& other) {
        m_handle = std::move(other.m_handle);
        m_msgSize = other.m_msgSize;

        assert(m_handle.get());

        other.m_handle = Handle();

        return *this;
    }

    ~Endpoint() = default;

    /////////////////////////////
    /// \brief Close Endpoint
    ///
    /// Destroy the handle to the endpoint,
    /// the actual endpoint will be destroyed when any peers destroy their handles
    /////////////////////////////
    inline void Close() {
        DestroyKObject(m_handle.get());

        m_handle = Handle();
    }

    inline operator handle_t() { return m_handle.get(); }

    /////////////////////////////
    /// \brief Get the Kernel Handle to the Endpoint
    ///
    /// Returns a Lemon OS handle ID pointing to the endpoint.
    ///
    /// \return Handle ID of endpoint
    /////////////////////////////
    inline const Handle& GetHandle() { return m_handle; }

    /////////////////////////////
    /// \brief Get the message size
    ///
    /// \return Maximum message size
    /////////////////////////////
    inline uint16_t GetMessageSize() const { return m_msgSize; }

    inline long Queue(uint64_t id, const uint8_t* data, uint16_t size) {
        return EndpointQueue(m_handle.get(), id, size, reinterpret_cast<uintptr_t>(data));
    }

    inline long Queue(uint64_t id, uint64_t data, uint16_t size) {
        return EndpointQueue(m_handle.get(), id, size, data);
    }

    inline long Queue(const Message& m) {
        return EndpointQueue(m_handle.get(), m.id(), m.length(), reinterpret_cast<uintptr_t>(m.data()));
    }

    inline long Poll(Message& m) {
        uint64_t id;
        uint16_t size;
        uint8_t* data = new uint8_t[m_msgSize];
        long ret = EndpointDequeue(m_handle.get(), &id, &size, data);

        if (ret > 0) {
            m.Set(data, size, id);
        } else {
            delete[] data;
        }
        return ret;
    }

    inline long Call(const Message& call, Message& rmsg, uint64_t id) {
        uint16_t size = call.length();
        uint8_t* data = new uint8_t[m_msgSize];

        long ret = EndpointCall(m_handle.get(), call.id(), reinterpret_cast<uintptr_t>(call.data()), id,
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

        long ret = EndpointCall(m_handle.get(), call.id(), reinterpret_cast<uintptr_t>(call.data()), id,
                                reinterpret_cast<uintptr_t>(call.data()), &size);

        if (!ret) {
            call.Set(data, size, id);
        }
        return ret;
    }

protected:
    Handle m_handle;
    uint16_t m_msgSize = 512;
};
}; // namespace Lemon
