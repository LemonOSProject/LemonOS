#include <Lemon/IPC/Endpoint.h>
#include <Lemon/IPC/Interface.h>

#include <Lemon/System/IPC.h>
#include <errno.h>

namespace Lemon {
const char* const EndpointException::errorStrings[] = {
    "Error: Unknown Endpoint Error",
    "Error Creating Endpoint: Could not resolve path of interface",
    "Error: Invalid endpoint handle",
    "Error: Endpoint not connected",
};

const char* const InterfaceException::errorStrings[] = {
    "Unknown Interface Error",
    "Error Creating Interface: Interface Name Exists",
    "Error Creating Interface: Invalid Service Handle",
    "Error Creating Interface",
};

Interface::Interface(const Handle& service, const char* name, uint16_t msgSize) : m_serviceHandle(service) {
    handle_t handle = CreateInterface(service.get(), name, msgSize);
    m_msgSize = msgSize;

    if (handle <= 0) {
        if (handle == -EEXIST) {
            throw InterfaceException(InterfaceException::InterfaceCreateExists);
        } else if (handle == -EINVAL) {
            throw InterfaceException(InterfaceException::InterfaceCreateInvalidService);
        } else {
            throw InterfaceException(InterfaceException::InterfaceCreateOther);
        }
    }

    m_interfaceHandle = Handle(handle);
    m_dataBuffer = new uint8_t[msgSize];
}

void Interface::RegisterObject(const std::string& name, int id) { m_objects[name] = id; }

long Interface::Poll(Lemon::Handle& client, Message& m) {
    handle_t newIf;
    while ((newIf = InterfaceAccept(m_interfaceHandle.get()))) { // Accept any incoming connections
        if (newIf > 0) {
            m_rawEndpoints.push_back(newIf);
            m_endpoints.push_back(Handle(newIf));

            for (Waiter* waiter : waiters) {
                waiter->RepopulateHandles(); // Repopulate handles
            }
        }
    }

    if (m_queue.size() > 0) { // Check for messages on the queue
        auto& front = m_queue.front();

        client = front.client;
        m.Set(front.data, front.length, front.id);

        m_queue.pop_front();
        return 1;
    }

    for (auto it = m_endpoints.begin(); !m_endpoints.empty() && it != m_endpoints.end(); it++) {
        InterfaceMessageInfo msg{*it, 0, nullptr, 0};

        while (long ret = EndpointDequeue(it->get(), &msg.id, &msg.length, m_dataBuffer)) {
            if (ret < 0) { // We have probably disconnected
                m_endpoints.erase(it);
                RepopulateRawHandles();

                msg.id = MessagePeerDisconnect;
                m_queue.push_back(std::move(msg));

                for (Waiter* waiter : waiters) {
                    waiter->RepopulateHandles();
                }
                break;
            }

            msg.data = m_dataBuffer;
            m_queue.push_back(msg);

            m_dataBuffer = new uint8_t[m_msgSize];
        }
    }

    if (m_queue.size() > 0) {
        auto& front = m_queue.front();

        client = std::move(front.client);
        m.Set(front.data, front.length, front.id);

        m_queue.pop_front();
        return 1;
    } else {
        return 0;
    }
}

void Interface::RepopulateRawHandles() {
    m_rawEndpoints.clear();
    for (auto& handle : m_endpoints) {
        m_rawEndpoints.push_back(handle.get());
    }
}
} // namespace Lemon
