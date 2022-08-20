# Inter-process Communication in Lemon OS 

## Kernel-Level IPC interfaces
### Services
A service is a container allowing a single application to host multiple interfaces. Services also allow clients to identify and connect to an interface. An example of a service/interface pair is `lemon.lemonwm/Instance`.

### Interfaces
An interface is a way for clients to open a connection to a server. Two operations can be performed on an interface.
- ```SysInterfaceAccept``` - Accept incoming connections on an interface
- ```SysInterfaceConnect``` - Connect to an interface 

### Endpoints
An endpoint can recieve from an send messages to a peer. Each endpoint has its own queue which can be read from and has a peer in which messages can be sent to.

## LibLemon IPC API

## Interface Description Language
See [LIC (Lemon Interface Compiler)](../../Build/lic.md)

### Interfaces
The `interface` keyword defines an interface. An interface consists of a server and a client. Clients can send *requests*, servers can reply with *responses*. Each request and response is given a unique ID starting from 100.

For each interface lic will generate the following:
- A *server* class containing:
    - An enum with all request and response IDs (public)
    - A struct containing response data for each response (public)
    - Pure virtual callbacks for each request (protected)
- A *client* class (inheriting from `Lemon::Endpoint`) containing:
    - A function for each request (public)

### Types
`s8, s16, s32, s64` Signed integer \
`u8. u16, u32. u64` Unsigned integer \
`bool` boolean \
`string` string \

### Requests

`AsynchronousRequest(string firstParameter, ...)` \
`SynchronousRequest(bool firstParameter, ...) -> (s64 firstReturnParameter, ...)` \

Asynchronous requests do not wait for a reply (known as 'fire and forget'), these may be used in situation where there is no response needed from the server and that in the event that the server mishandles the message, the client will not have any issues.

Synchronous requests are useful when a reply is necessary or it is crucial that the server handles the message correctly.

The buffer for each request and response is stored on the stack and passed over to the endpoint.