#pragma once

#include <stdint.h>

#define XHCI_PORT_OFFSET 0x400

#define USB_STS_HCH (1 << 0) // HCHalted - 0 if CMD_RS is 1
#define USB_STS_HSE (1 << 2) // Host System Error - set to 1 on error
#define USB_STS_EINT (1 << 3) // Event Interrupt
#define USB_STS_PCD (1 << 4) // Port change detect
#define USB_STS_SSS (1 << 8) // Save State Status - 1 when CMD_CSS is 1
#define USB_STS_RSS (1 << 9) // Restore State Status - 1 when CMD_CRS is 1
#define USB_STS_SRE (1 << 10) // Save/Restore Error - 1 when error during save or restore operation
#define USB_STS_CNR (1 << 11) // Controller Not Ready - 0 = Ready, 1 = Not Ready
#define USB_STS_HCE (1 << 12) // Host Controller Error

#define USB_CCR_RCS (1 << 0) // Ring Cycle State
#define USB_CCR_CS (1 << 1) // Command Stop
#define USB_CCR_CA (1 << 2) // Command Abort
#define USB_CCR_CRR (1 << 3) // Command Ring Running

#define USB_CCR_PTR_LO 0xFFFFFFC0‬
#define USB_CCR_PTR 0xFFFFFFFFFFFFFFC0‬ // Command Ring Pointer

namespace USB{
    namespace XHCI{
        typedef struct {
            uint8_t capLength; // Capability Register Length
            uint8_t reserved;
            uint16_t hciVersion; // Interface Version Number
            uint32_t hcsParams1;
            uint32_t hcsParams2;
            uint32_t hcsParams3;
            uint32_t hccParams1;
            uint32_t dbOff; // Doorbell offset
            uint32_t rtsOff; // Runtime registers space offset
            uint32_t hccParams2;

            inline uint8_t MaxSlots(){ // Number of Device Slots
                return hcsParams1 & 0xFF;
            }

            inline uint16_t MaxIntrs(){ // Number of Interrupters
                return (hcsParams1 >> 8) & 0x3FF;
            } 

            inline uint8_t MaxPorts(){ // Number of Ports
                return (hcsParams1 >> 24) & 0xFF;
            }

            inline uint8_t IST(){ // Isochronous Scheduling Threshold
                return hcsParams2 & 0xF;
            }

            inline uint8_t ERSTMax(){ // Event Ring Segment Table Max
                return (hcsParams2 >> 4) & 0xF;
            }

            inline uint16_t MaxScratchpadBuffers(){
                return (((hcsParams2 >> 21) & 0x1F) << 5) | ((hcsParams2 >> 27) & 0x1F);
            }

            inline uint8_t U1DeviceExitLatency(){
                return hcsParams3 & 0xFF;
            }

            inline uint16_t U2DeviceExitLatency(){
                return (hcsParams3 >> 16) & 0xFFFF;
            }
        } __attribute__ ((packed)) xhci_cap_regs_t; // Capability Registers

        typedef struct {
            uint32_t usbCommand; // USB Command
            uint32_t usbStatus; // USB Status
            uint32_t pageSize; // Page Size
            uint8_t rsvd1[8];
            uint32_t deviceNotificationControl; // Device Notification Control
            uint64_t cmdRingCtl; // Command Ring Control
            uint8_t rsvd2[16];
            uint64_t devContextBaseAddrArrayPtr; // Device Context Base Address Array Pointer
            uint32_t configure; // Configure
        } __attribute__((packed)) xhci_op_regs_t; // Operational Registers
        
        typedef struct {
            uint32_t portSC; // Port Status and Control
            uint32_t portPMSC; // Power Management Status and Control
            uint32_t portLinkInfo; // Port Link Info
            uint32_t portHardwareLPMCtl; // Port Hardware LPM Control
        } __attribute__((packed)) xhci_port_regs_t; // Port Registers

        enum {
            SlotStateDisabledEnabled = 0, // Disabled / Enabled
            SlotStateDefault = 1,
            SlotStateAddressed = 2,
            SlotStateConfigured = 3,
        };

        typedef struct {
            // Offset 00h
            uint32_t routeString : 20; // Route string
            uint32_t speed : 4; // Speed (deprecated)
            uint32_t resvd : 1;
            uint32_t mtt : 1; // Multi TT
            uint32_t hub : 1; // Hub (1) or Function (0)
            uint32_t ctxEntries : 5; // Index of the last valid Endpoint Context
            // Offset 04h
            uint32_t maxExitLatency : 16;
            uint32_t rootHubPortNumber : 8; // Root Hub Port Number
            uint32_t numberOfPorts : 8; // If Hub then set to number of downstream facing ports
            // Offset 08h
            uint32_t parentHubSlotID : 8;
            uint32_t parentPortNumber : 8; // Parent Port Number
            uint32_t ttt : 2; // TT Think Time
            uint32_t resvdZ : 4;
            uint32_t interrupterTarget : 10; 
            // Offset 0Ch
            uint32_t usbDeviceAddress : 8; // Address assigned to USB device by the Host Controller
            uint32_t resvdZ_2 : 19;
            uint32_t slotState; : 5;
        } __attribute__((packed)) xhci_slot_context_t;

        int Initialize();
    }
}