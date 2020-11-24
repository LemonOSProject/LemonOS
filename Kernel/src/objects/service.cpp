#include <objects/service.h>

#include <errno.h>

ServiceFS* ServiceFS::instance = nullptr;

ServiceFS::ServiceFS(){
    if(instance){
        Log::Error("Instance of ServiceFS already created!");
    }
}

long ServiceFS::ResolveServiceName(FancyRefPtr<Service>& ref, const char* name){
    const char* separator = strchr(name, ' ');

    if(separator){
        for(auto& svc : services){
            if(strncmp(svc->GetName(), name, separator - name) == 0){
                ref = svc;
                return 0;
            }
        }
    } else {
        for(auto& svc : services){
            if(strcmp(svc->GetName(), name) == 0){
                ref = svc;
                return 0;
            }
        }
    }

    Log::Warning("Service %s not found!", name);
    return 1;
}

FancyRefPtr<Service> ServiceFS::CreateService(const char* name){
    auto svc = FancyRefPtr<Service>(new Service(name));

    services.add_back(svc);

    return svc;
}

Service::Service(const char* _name){
    name = strdup(_name);
}

long Service::CreateInterface(FancyRefPtr<MessageInterface>& rInterface, const char* name, uint16_t msgSize){
    if(strchr(name, '/')){
        Log::Warning("Service::CreateInterface: Invalid name \"%s\", cannot contain '/'");
        return -EINVAL;
    }

    for(auto& i : interfaces){
        if(strcmp(name, i->name) == 0){
            return -EEXIST;
        }
    }

    FancyRefPtr<MessageInterface> interface = FancyRefPtr<MessageInterface>(new MessageInterface(name, msgSize));

    interfaces.add_back(rInterface);

    return 0;
}