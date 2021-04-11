#pragma once

#include <Paging.h>
#include <Vector.h>
#include <VM/VMObject.h>

class AddressSpace {
public:
    VMObject* AddressToVMObject(uintptr_t address);
    VMObject* AllocateAnonymousVMObject(size_t size);
    AddressSpace* Fork();

protected:
    PageMap pageMap;
    Vector<VMObject> vmObjects;

    AddressSpace* parent;
};