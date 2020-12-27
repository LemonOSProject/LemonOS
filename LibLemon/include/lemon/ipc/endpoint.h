#pragma once

#include <lemon/types.h>
#include <lemon/system/ipc.h>
#include <lemon/system/kobject.h>

#include <lemon/ipc/message.h>

#include <stdint.h>

namespace Lemon{
    class EndpointException : public std::exception{
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
        EndpointException(int error){
            this->error = error;
        }

        const char* what() const noexcept{
            return errorStrings[error];
        }
    };

    class Endpoint{
    protected:
        handle_t handle = 0;
        uint16_t msgSize = 512;
    public:
        Endpoint() = default;

        Endpoint(handle_t h, uint16_t msgSize){
            handle = h;
            this->msgSize = msgSize;
        }

        Endpoint(handle_t h){
            handle = h;

            LemonEndpointInfo endpInfo;
            if(long ret = EndpointInfo(h, endpInfo); ret){
                if(ret == -EINVAL){
                    printf("[LibLemon] Invalid endpoint handle!\n");
                    throw new EndpointException(EndpointException::EndpointInvalidHandle);
                } else {
                    printf("[LibLemon] Endpoint error %ld\n!", ret);
                    throw new EndpointException(EndpointException::EndpointUnknownError);
                }
            }
            this->msgSize = endpInfo.msgSize;
        }

        Endpoint(const std::string& path){
            Endpoint(path.c_str());
        }

        Endpoint(const char* path){
            handle = InterfaceConnect(path);

            if(handle <= 0){
                printf("[LibLemon] Error %ld connecting to interface %s!\n", handle, path);
                throw new EndpointException(EndpointException::EndpointInterfacePathResolutionError);
            }

            LemonEndpointInfo endpInfo;
            if(long ret = EndpointInfo(handle, endpInfo); ret){
                if(ret == -EINVAL){
                    throw new EndpointException(EndpointException::EndpointInvalidHandle);
                } else {
                    throw new EndpointException(EndpointException::EndpointUnknownError);
                }
            }
            this->msgSize = endpInfo.msgSize;
        }

        ~Endpoint(){
            
        }

        inline void Close(){
            DestroyKObject(handle);

            handle = 0;
        }

        inline operator handle_t(){
            return handle;
        }

        inline long Queue(uint64_t id, uint8_t* data, uint16_t size) const{
            return EndpointQueue(handle, id, size, reinterpret_cast<uintptr_t>(data));
        }

        inline long Queue(uint64_t id, uint64_t data, uint16_t size) const{
            return EndpointQueue(handle, id, size, data);
        }

        inline long Queue(const Message& m) const{
            return EndpointQueue(handle, m.id(), m.length(), (m.length() > 8) ? reinterpret_cast<uintptr_t>(m.data()) : *(reinterpret_cast<const uint64_t*>(m.data())));
        }

        inline long Poll(Message& m) const{
            uint64_t id;
            uint16_t size;
            uint8_t* data = new uint8_t[msgSize];
            long ret = EndpointDequeue(handle, &id, &size, data);

            if(ret > 0){
                m.Set(data, size, id);
            } else {
                delete[] data;
            }
            return ret;
        }

        inline long Call(const Message& call, Message& rmsg, uint64_t id) const{
            uint16_t size = call.length();
            uint8_t* data = new uint8_t[msgSize];

            long ret = EndpointCall(handle, call.id(), (call.length() > 8) ? reinterpret_cast<uintptr_t>(call.data()) : *(reinterpret_cast<const uint64_t*>(call.data())), id, reinterpret_cast<uintptr_t>(data), &size);

            if(!ret){
                rmsg.Set(data, size, id);
            } else {
                delete[] data;
            }
            return ret;
        }
    };
};