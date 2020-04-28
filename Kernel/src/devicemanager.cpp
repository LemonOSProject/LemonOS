#include <devicemanager.h>

namespace DeviceManager{
    List<Device*>* devices;

    void Init(){
        devices = new List<Device*>();
    }
}