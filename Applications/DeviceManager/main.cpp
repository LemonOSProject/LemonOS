#include <lemon/gui/window.h>
#include <lemon/gui/model.h>

#include <lemon/system/device.h>

Lemon::GUI::Window* window;

struct Device{
	std::string name;
	std::string instanceName;
	long type;
	long id;
};

class DeviceModel : public Lemon::GUI::DataModel {
public:
    int ColumnCount() const {
        return columns.size();
    }

    int RowCount() const {
        return devices.size();
    }

    const std::string& ColumnName(int column) const {
        return columns.at(column).Name();
    }

	Lemon::GUI::Variant GetData(int row, int column){
		Device& dev = devices.at(row);

		switch(column){
		case 0:
			return dev.name;
		case 1:
			return dev.instanceName;
		case 2:
			switch(dev.type){
				case Lemon::DeviceTypeUNIXPseudo:
					return "UNIX";
				case Lemon::DeviceTypeUNIXPseudoTerminal:
					return "UNIX Pseudoterminal";
				case Lemon::DeviceTypeKernelLog:
					return "Kernel Log";
				case Lemon::DeviceTypeGenericPCI:
					return "Generic PCI Device";
				case Lemon::DeviceTypeGenericACPI:
					return "ACPI Device";
				case Lemon::DeviceTypeStorageController:
					return "Storage Controller";
				case Lemon::DeviceTypeStorageDevice:
					return "Storage Device";
				case Lemon::DeviceTypeStoragePartition:
					return "Storage Partition";
				case Lemon::DeviceTypeNetworkAdapter:
					return "Network Adapter";
				case Lemon::DeviceTypeUSBController:
					return "USB Controller";
				case Lemon::DeviceTypeGenericUSB:
					return "Generic USB Device";
				case Lemon::DeviceTypeLegacyHID:
					return "Legacy HID";
				case Lemon::DeviceTypeUSBHID:
					return "USB HID";
			}
		default:
			return 0;
		}
	}

    int SizeHint(int column){
        switch (column) {
        case 0: // Name
        case 1: // Instance name
		case 2: // Type
            return 132;
        default:
            return 76;
        }
    }

	void Refresh(){
		long deviceCount = Lemon::GetRootDeviceCount();
		int64_t rootDevices[deviceCount];

		deviceCount = Lemon::EnumerateRootDevices(0, deviceCount, rootDevices);

		devices.clear();
		for(long i = 0; i < deviceCount; i++){
			std::string name;
			std::string instanceName;

			long type = Lemon::DeviceGetType(rootDevices[i]);
			if(type < 0 || type == Lemon::DeviceTypeNetworkStack){
				continue; // Error getting device type
			}

			char nameBuf[NAME_MAX];
			if(int e = Lemon::DeviceGetName(rootDevices[i], nameBuf, NAME_MAX); e){
				continue; // Error getting device name
			}

			name = nameBuf;

			if(int e = Lemon::DeviceGetInstanceName(rootDevices[i], nameBuf, NAME_MAX); e){
				continue; // Error getting device instance name
			}

			instanceName = nameBuf;

			devices.push_back({ .name = std::move(name), .instanceName = std::move(instanceName), .type = type, .id = rootDevices[i]});
		}
	}
private:
	std::vector<Device> devices;
    std::vector<Column> columns = { Column("Name"), Column("Instance"), Column("Type") };
};

int main(int argc, char** argv){
	window = new Lemon::GUI::Window("Device Manager", {400, 480}, 0, Lemon::GUI::WindowType::GUI);

	Lemon::GUI::ListView* lv = new Lemon::GUI::ListView({0, 0, 0, 0});
	lv->SetLayout(Lemon::GUI::Stretch, Lemon::GUI::Stretch);

	window->AddWidget(lv);

	DeviceModel model;
	lv->SetModel(&model);

	while(!window->closed){
		Lemon::LemonEvent ev;
		while(window->PollEvent(ev)){
			window->GUIHandleEvent(ev);
		}

		window->Paint();
		window->WaitEvent();
	}

	return 0;
}