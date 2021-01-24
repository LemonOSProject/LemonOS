# NetworkGovernor

## Configuration
NetworkGovernor reads its configuration file from `/system/lemon/networkgovernor.json`

### DHCP Configuration
DHCP configuration is under the `dhcp` property. For each interface you would like to configure, create an object with the key corresponding to the kernel interface name (e.g. elk0s2).

`ipAddress` Interface IP address \
`subnetMask` Interface Subnet mask \
`defaultGateway` Gateway (Router) IP address \
`dnsServers` Array of DNS servers

Example:
```
{
    "dhcp" : {
        "elk0s2" : {
            "ipAddress" : "10.0.1.69",
            "subnetMask" : "255.0.0.0",
            "defaultGateway" : "10.0.1.1",
            "dnsServers" : [
                "1.1.1.1",
                "10.0.1.1"
            ]
        }
    }
}
```