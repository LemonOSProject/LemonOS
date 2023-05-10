#pragma once

#include <lemon/ipc/Message.h>
#include <string.h>

#include <list>
#include <map>
#include <vector>

#include <lemon/system/Handle.h>
#include <lemon/system/IPC.h>
#include <lemon/system/Waitable.h>
#include <lemon/types.h>

#include <exception>

namespace Lemon {
class InterfaceException : public std::exception {
public:
    enum {
        InterfaceUnknownError,
        InterfaceCreateExists,
        InterfaceCreateInvalidService,
        InterfaceCreateOther,
    };

protected:
    int error = 0;
    static const char* const errorStrings[];

public:
    InterfaceException(int error) { this->error = error; }

    const char* what() const noexcept { return errorStrings[error]; }
};

class Interface : public Waitable {
public:
    /////////////////////////////
    /// \brief Interface::Interface(service, name, msgSize) - Create a new interface
    ///
    /// Create a new interface. Interfaces allow clients to open connections to a service
    ///
    /// \param service (handle_t) Handle ID of the service hosting the interface
    /// \param name (const char*) Name of the interface,
    /// \param msgSize (uint16_t) Maximum message size for all connections
    /////////////////////////////
    Interface(const Handle& service, const char* name, uint16_t msgSize);

    void RegisterObject(const std::string& name, int id);

    long Poll(Handle& client, Message& m);
    void Wait();

    inline const Handle& GetHandle() { return m_interfaceHandle; }
    void GetAllHandles(std::vector<le_handle_t>& v) {
        v.insert(v.end(), m_rawEndpoints.begin(), m_rawEndpoints.end());
        v.push_back(m_interfaceHandle.get());
    }

private:
    Handle m_serviceHandle;
    Handle m_interfaceHandle;

    // Repopulate the std::vector of raw handles
    void RepopulateRawHandles();

protected:
    struct InterfaceMessageInfo {
        Handle client;
        uint64_t id;
        uint8_t* data = nullptr;
        uint16_t length = 0;
    };

    std::map<std::string, int> m_objects;
    std::vector<le_handle_t> m_rawEndpoints;
    std::list<Handle> m_endpoints;
    uint16_t m_msgSize;
    uint8_t* m_dataBuffer = nullptr;

    std::deque<InterfaceMessageInfo> m_queue;
};
} // namespace Lemon
