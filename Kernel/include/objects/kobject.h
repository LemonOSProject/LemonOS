#pragma once

#include <debug.h>
#ifdef DEBUG_KOBJECTS
    #include <logging.h>
#endif

#include <stdint.h>
#include <ttraits.h>

extern int ko_nextOID;

class KernelObject{
protected:
    int64_t oid = -1;

public:
    KernelObject(){
        oid = ko_nextOID++;
    }

    inline int64_t ObjectID() { return oid; }

    virtual ~KernelObject(){

    }
}