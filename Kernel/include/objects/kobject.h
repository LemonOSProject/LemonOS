#pragma once

#include <debug.h>
#ifdef DEBUG_KOBJECTS
    #include <logging.h>
#endif

#include <stdint.h>
#include <string.h>

typedef long kobject_id_t;

#define KOBJECT_ID_MESSAGE_ENDPOINT 1
#define KOBJECT_ID_INTERFACE 2
#define KOBJECT_ID_SERVICE 3

class KernelObject{
protected:
    int64_t oid = -1;
    static int64_t nextOID;

public:
    KernelObject(){
        oid = nextOID++;
    }

    inline int64_t ObjectID() { return oid; }

    virtual kobject_id_t InstanceTypeID() const = 0;

    inline bool IsType(kobject_id_t id){
        return InstanceTypeID() == id;
    }

    virtual ~KernelObject(){

    }
};