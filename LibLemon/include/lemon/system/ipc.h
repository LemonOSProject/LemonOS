#pragma once 

#include <lemon/types.h>
#include <lemon/syscall.h>

namespace Lemon{
    struct LemonEndpointInfo{
        uint16_t msgSize;
    };

    /////////////////////////////
    /// \brief CreateService (name) - Create a service
    ///
    /// Create a new service. Services are essentially named containers for interfaces, allowing one program to implement multiple interfaces under a service.
    ///
    /// \param name (const char*) Service name to be used, must be unique
    ///
    /// \return Handle ID on success, error code on failure
    /////////////////////////////
    inline handle_t CreateService(const char* name){
        return syscall(SYS_CREATE_SERVICE, name);
    }

    /////////////////////////////
    /// \brief CreateInterface (service, name, msgSize) - Create a new interface
    ///
    /// Create a new interface. Interfaces allow clients to open connections to a service
    ///
    /// \param service (handle_t) Handle ID of the service hosting the interface
    /// \param name (const char*) Name of the interface, 
    /// \param msgSize (uint16_t) Maximum message size for all connections
    ///
    /// \return Handle ID of service on success, negative error code on failure
    /////////////////////////////
    inline handle_t CreateInterface(const handle_t svc, const char* name, const uint16_t msgSize){
        return syscall(SYS_CREATE_INTERFACE, svc, name, msgSize);
    }

    /////////////////////////////
    /// \brief InterfaceAccept (interface) - Accept connections on an interface
    ///
    /// Accept a pending connection on an interface and return a new MessageEndpoint
    ///
    /// \param interface (handle_t) Handle ID of the interface
    ///
    /// \return Handle ID of endpoint on success, 0 when no pending connections, negative error code on failure
    /////////////////////////////
    inline handle_t InterfaceAccept(const handle_t interface){
        return syscall(SYS_INTERFACE_ACCEPT, interface);
    }

    /////////////////////////////
    /// \brief InterfaceConnect (path) - Open a connection to an interface
    ///
    /// Open a new connection on an interface and return a new MessageEndpoint
    ///
    /// \param interface (const char*) Path of interface in format servicename/interfacename (e.g. lemon.lemonwm/wm)
    ///
    /// \return Handle ID of endpoint on success, negative error code on failure
    /////////////////////////////
    inline handle_t InterfaceConnect(const char* path){
        return syscall(SYS_INTERFACE_CONNECT, path);
    }

    /////////////////////////////
    /// \brief EndpointQueue (endpoint, id, size, data) - Queue a message on an endpoint
    ///
    /// Queues a new message on the specified endpoint.
    ///
    /// \param endpoint (handle_t) Handle ID of specified endpoint
    /// \param id (uint64_t) Message ID
    /// \param size (uint64_t) Message Size
    /// \param data (uint8_t*/uint64_t) Message data, if size <= 8 then treated as an integer containing message data, if size > 8 then treated as a pointer to message data
    ///
    /// \return 0 on success, negative error code on failure
    /////////////////////////////
    __attribute__((always_inline)) inline long EndpointQueue(const handle_t endpoint, const uint64_t id, const uint16_t size, const uintptr_t data){
        return syscall(SYS_ENDPOINT_QUEUE, endpoint, id, size, data);
    }

    /////////////////////////////
    /// \brief EndpointDequeue (endpoint, id, size, data)
    ///
    /// Accept a pending connection on an interface and return a new MessageEndpoint
    ///
    /// \param endpoint (handle_t) Handle ID of specified endpoint
    /// \param id (uint64_t*) Returned message ID
    /// \param size (uint32_t*) Returned message Size
    /// \param data (uint8_t*) Message data buffer
    ///
    /// \return 0 on empty, 1 on success, negative error code on failure
    /////////////////////////////
    __attribute__((always_inline)) inline long EndpointDequeue(const handle_t endpoint, uint64_t* id, uint16_t* size, uint8_t* data){
        return syscall(SYS_ENDPOINT_DEQUEUE, endpoint, id, size, data);
    }

    /////////////////////////////
    /// \brief EndpointCall (endpoint, id, data, rID, rData, size, timeout)
    ///
    /// Accept a pending connection on an interface and return a new MessageEndpoint
    ///
    /// \param endpoint (handle_id_t) Handle ID of specified endpoint
    /// \param id (uint64_t) id of message to send
    /// \param data (uint8_t*/uint64_t) Message data to be sent, if size <= 8 then treated as an integer containing message data, if size > 8 then treated as a pointer to message data
    /// \param rID (uint64_t) id of expected returned message
    /// \param rData (uint8_t*) Return message data buffer
    /// \param size (uint32_t*) Pointer containing size of message to be sent and size of returned message
    /// \param timeout (int64_t) timeout in us
    ///
    /// \return 0 on success, negative error code on failure
    /////////////////////////////
    __attribute__((always_inline)) inline long EndpointCall(const handle_t endpoint, const uint64_t id, const uintptr_t data, const uint64_t rID, uintptr_t rData, uint16_t* size){
        return syscall(SYS_ENDPOINT_CALL, endpoint, id, data, rID, rData, size);
    }

    /////////////////////////////
    /// \brief SysEndpointInfo (endpoint, info)
    ///
    /// Get endpoint information
    ///
    /// \param endpoint Endpoint handle
    /// \param info Pointer to endpoint info struct
    ///
    /// \return 0 on success, negative error code on failure
    /////////////////////////////
    __attribute__((always_inline)) inline long EndpointInfo(const handle_t endp, LemonEndpointInfo& info){
        return syscall(SYS_ENDPOINT_INFO, endp, &info);
    }
}