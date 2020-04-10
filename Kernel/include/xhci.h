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

        int Initialize();
    }
}