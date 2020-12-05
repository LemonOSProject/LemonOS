#pragma once 

#include <lemon/types.h>
#include <lemon/syscall.h>

namespace Lemon{
    inline handle_id_t CreateService(const char* name){

    }

    inline handle_id_t CreateInterface(handle_id_t svc, const char* name){

    }

    inline handle_id_t InterfaceAccept(handle_id_t interface){

    }

    inline handle_id_t InterfaceConnect(const char* path){

    }

    inline long EndpointQueue(handle_id_t endpoint){

    }

    inline long EndpointDequeue(handle_id_t endpoint){

    }
}