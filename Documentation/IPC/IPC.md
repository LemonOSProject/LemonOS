# Inter-process Communication in Lemon OS 

## Kernel-Level IPC interfaces
### Services
A service is a container allowing a single executable to host multiple interface.

### Interfaces
An interface is a way for clients to open a connection to a server. Two operations can be performed on an interface.
- ```SysInterfaceAccept``` - Accept incoming connections on an interface
- ```SysInterfaceConnect``` - Connect to an interface 

### Endpoints
An endpoint can recieve from an send messages to a peer. Each endpoint has its own queue which can be read from and has a peer in which messages can be sent to.

## LibLemon IPC API

## Interface Description Language (Work in progress)
