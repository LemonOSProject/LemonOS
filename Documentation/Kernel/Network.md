# Kernel Network Configuration Interface

`lemon.kernel/Networking`

## Synchronous Requests
GetNumberOfInterfaces() -> (s32 status, u32 num)

GetInterfaceConfiguration() -> (u32 ipAdddress, u32 defaultGateway, u32 subnetMask)
GetInterfaceMACAddress() -> (struct MACAddress macAddress)
GetInterfaceName() -> (string name)
GetInterfaceLink() -> (bool link)

SetInterfaceConfiguration(u32 ipAdddress, u32 defaultGateway, u32 subnetMask) -> (s32 status)