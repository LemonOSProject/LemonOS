#pragma once

#include <fs/fsvolume.h>

#include <objects/kobject.h>
#include <objects/interface.h>

#include <vector.h>

class Service;

class ServiceFS : public fs::FsVolume {
protected:
    friend class Service;
    static ServiceFS* instance;

public:
    List<FancyRefPtr<Service>> services;

    static void Initialize(){
        instance = new ServiceFS();
    }

    ServiceFS();

    static ServiceFS* Instance(){
        assert(instance);

        return instance;
    }

    long ResolveServiceName(FancyRefPtr<Service>& ref, const char* name);
    FancyRefPtr<Service> CreateService(const char* name);
};

class Service final : public KernelObject{
protected:
    char* name;
    List<FancyRefPtr<MessageInterface>> interfaces;

public:
    Service(const char* _name);
    ~Service();

    long CreateInterface(FancyRefPtr<MessageInterface>& rInterface, const char* name, uint16_t msgSize);
    long ResolveInterface(FancyRefPtr<MessageInterface>& interface, const char* name);

    const char* GetName() { return name; };

    inline static constexpr const char* TypeID() { return "Service"; }
    const char* InstanceTypeID() const { return TypeID(); }
};