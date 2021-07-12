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

Interface::Interface(handle_t service, const char* name, uint16_t msgSize) {
    interfaceHandle = CreateInterface(service, name, msgSize);
    (void)(service + name);
    this->msgSize = msgSize;

    if (interfaceHandle <= 0) {
        if (interfaceHandle == -EEXIST) {
            throw InterfaceException(InterfaceException::InterfaceCreateExists);
        } else if (interfaceHandle == -EINVAL) {
            throw InterfaceException(InterfaceException::InterfaceCreateInvalidService);
        } else {
            throw InterfaceException(InterfaceException::InterfaceCreateOther);
        }
    }

    dataBuffer = new uint8_t[msgSize];
}

void Interface::RegisterObject(const std::string& name, int id) { objects[name] = id; }

long Interface::Poll(handle_t& client, Message& m) {
    handle_t newIf;
    while ((newIf = InterfaceAccept(interfaceHandle))) { // Accept any incoming connections
        if (newIf > 0) {
            endpoints.push_back(newIf);

            for (Waiter* waiter : waiters) {
                waiter->RepopulateHandles(); // Repopulate handles
            }
        }
    }

    if (queue.size() > 0) { // Check for messages on the queue
        auto& front = queue.front();

        client = front.client;
        m.Set(front.data, front.length, front.id);

        queue.pop_front();
        return 1;
    }

    for (auto it = endpoints.begin(); !endpoints.empty() && it != endpoints.end(); it++) {
        handle_t& endpoint = *it;
        InterfaceMessageInfo msg = {endpoint, 0, nullptr, 0};

        while (long ret = EndpointDequeue(endpoint, &msg.id, &msg.length, dataBuffer)) {
            if (ret < 0) { // We have probably disconnected
                if (ret == -ENOTCONN) {
                    DestroyKObject(endpoint);
                }

                endpoints.erase(it);

                queue.push_back({.client = endpoint, .id = MessagePeerDisconnect, .data = nullptr, .length = 0});

                for (Waiter* waiter : waiters) {
                    waiter->RepopulateHandles();
                }
                break;
            }

            msg.data = dataBuffer;
            queue.push_back(msg);

            dataBuffer = new uint8_t[msgSize];
        }
    }

    if (queue.size() > 0) {
        auto& front = queue.front();

        client = front.client;
        m.Set(front.data, front.length, front.id);

        queue.pop_front();
        return 1;
    } else {
        return 0;
    }
}
} // namespace Lemon
