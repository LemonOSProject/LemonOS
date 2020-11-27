#pragma once

#include <debug.h>
#ifdef DEBUG_KOBJECTS
    #include <logging.h>
#endif

#include <stdint.h>
#include <string.h>

class KernelObject{
protected:
    int64_t oid = -1;
    static int64_t nextOID;

public:
    KernelObject(){
        oid = nextOID++;
    }

    inline int64_t ObjectID() { return oid; }

    virtual const char* InstanceTypeID() const = 0;

    inline bool IsType(const char* id){
        return !strcmp(InstanceTypeID(), id);
    }

    virtual ~KernelObject(){

    }
};