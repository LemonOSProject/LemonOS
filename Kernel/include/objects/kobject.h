#pragma once

#include <debug.h>
#ifdef DEBUG_KOBJECTS
    #include <logging.h>
#endif

#include <stdint.h>
#include <ttraits.h>

class KernelObject{
protected:
    int64_t oid = -1;
    static int64_t nextOID;

public:
    KernelObject(){
        oid = nextOID++;
    }

    inline int64_t ObjectID() { return oid; }

    virtual ~KernelObject(){

    }
};