#pragma once

#include <lemon/ipc/message.h>
#include <string.h>

#include <list>
#include <vector>
#include <map>

#include <lemon/types.h>
#include <lemon/system/ipc.h>
#include <lemon/system/waitable.h>

#include <exception>

namespace Lemon{
    class InterfaceException : public std::exception{
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
        InterfaceException(int error){
            this->error = error;
        }

        const char* what() const noexcept{
            return errorStrings[error];
        }
    };

    class Interface : public Waitable{
    private:
        handle_t interfaceHandle;
    protected:
        struct InterfaceMessageInfo{
            handle_t client = -1;
            uint64_t id;
            uint8_t* data = nullptr;
            uint16_t length = 0;
        };

        std::map<std::string, int> objects;
        std::list<handle_t> endpoints;
        uint16_t msgSize;
        uint8_t* dataBuffer = nullptr;

        std::deque<InterfaceMessageInfo> queue;
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
        Interface(handle_t service, const char* name, uint16_t msgSize);

        /////////////////////////////
        /// \brief Interface::Queue(endpoint, m) - Create a new interface
        ///
        /// Create a new interface. Interfaces allow clients to open connections to a service
        ///
        /// \param endpoint (handle_t) Handle ID of endpoint
        /// \param m (const Message&) Message to send
        /////////////////////////////
        inline long Queue(handle_t h, Message& m) const{
            return EndpointQueue(h, m.id(), m.length(), reinterpret_cast<uintptr_t>(m.data()));
        }

        void RegisterObject(const std::string& name, int id);

        long Poll(handle_t& client, Message& m);
        void Wait();
        
        inline const handle_t& GetHandle() const { return interfaceHandle; }
        void GetAllHandles(std::vector<handle_t>& v) const {
            v.insert(v.end(), endpoints.begin(), endpoints.end());
            v.push_back(interfaceHandle);
        }
    };
}