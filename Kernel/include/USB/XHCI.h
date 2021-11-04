#pragma once

#include <Lock.h>
#include <PCI.h>
#include <stdint.h>

#define XHCI_PORT_OFFSET 0x400

#define USB_CMD_RS (1 << 0)    // Run/Stop
#define USB_CMD_HCRST (1 << 1) // Host Controller Reset
#define USB_CMD_INTE (1 << 2)  // Interrupter enable
#define USB_CMD_HSEE (1 << 3)  // Host System Error enable

#define USB_STS_HCH (1 << 0)  // HCHalted - 0 if CMD_RS is 1
#define USB_STS_HSE (1 << 2)  // Host System Error - set to 1 on error
#define USB_STS_EINT (1 << 3) // Event Interrupt
#define USB_STS_PCD (1 << 4)  // Port change detect
#define USB_STS_SSS (1 << 8)  // Save State Status - 1 when CMD_CSS is 1
#define USB_STS_RSS (1 << 9)  // Restore State Status - 1 when CMD_CRS is 1
#define USB_STS_SRE (1 << 10) // Save/Restore Error - 1 when error during save or restore operation
#define USB_STS_CNR (1 << 11) // Controller Not Ready - 0 = Ready, 1 = Not Ready
#define USB_STS_HCE (1 << 12) // Host Controller Error

#define USB_CFG_MAXSLOTSEN (0xFF) // Max slots enabled
#define USB_CFG_U3E (1 << 8)      // U3 Entry Enable
#define USB_CFG_CIE (1 << 9)      // Configuration Information Enable

#define USB_CCR_RCS (1 << 0) // Ring Cycle State
#define USB_CCR_CS (1 << 1)  // Command Stop
#define USB_CCR_CA (1 << 2)  // Command Abort
#define USB_CCR_CRR (1 << 3) // Command Ring Running

#define USB_CCR_PTR_LO 0xFFFFFFC0‬
#define USB_CCR_PTR 0xFFFFFFFFFFFFFFC0‬ // Command Ring Pointer

#define XHCI_PORTSC_CCS (1 << 0) // Current Connect Status
#define XHCI_PORTSC_PED (1 << 1) // Port Enabled/Disabled
#define XHCI_PORTSC_OCA (1 << 3) // Overcurrent Active
#define XHCI_PORTSC_PR                                                                                                 \
    (1 << 4) // Port Reset - When software writes a ‘1’ to this bit generating a ‘0’ to ‘1’ transition, the bus reset
             // sequence is initiated
#define XHCI_PORTSC_PP (1 << 9)   // Port Power - When 0 the port is powered off
#define XHCI_PORTSC_CSC (1 << 17) // Connect Status Change
#define XHCI_PORTSC_PEC (1 << 18) // Port Enabled/Disabled Change
#define XHCI_PORTSC_PRC (1 << 21) // Port Reset Change
#define XHCI_PORTSC_WPR (1 << 31) // On USB3 ports warm reset

#define XHCI_INT_ERDP_BUSY (1 << 3)

#define XHCI_TRB_SIZE 16
#define XHCI_EVENT_RING_SEGMENT_TABLE_ENTRY_SIZE 16

namespace USB {
class XHCIController : private PCIDevice {
public:
public:
    enum Status {
        ControllerNotInitialized,
        ControllerInitialized,
    };

    XHCIController(const PCIInfo& dev);
    ~XHCIController();

    inline Status GetControllerStatus() { return controllerStatus; }

    static int Initialize();

protected:
    friend void XHCIIRQHandler(XHCIController* xHC, RegisterContext* r);

    enum XHCIExtendedCapabilities {
        XHCIExtCapLegacySupport = 1,
        XHCIExtCapSupportedProtocol = 2,
        XHCIExtCapExtendedPowerManagement = 3,
        XHCIExtCapIOVirtualization = 4,
        XHCIExtCapMessageInterrupt = 5,
        XHCIExtCapLocalMemory = 6,
        XHCIExtCapUSBDebug = 10,
        XHCIExtCapExtendedMessageInterrupt = 17,
    };

    typedef struct {
        uint8_t capLength; // Capability Register Length
        uint8_t reserved;
        uint16_t hciVersion; // Interface Version Number
        uint32_t hcsParams1;
        uint32_t hcsParams2;
        uint32_t hcsParams3;
        union {
            uint32_t hccParams1;
            struct {
                uint32_t addressCap64 : 1;     // 64-bit Addressing Capability
                uint32_t bwNegotationCap : 1;  // BW Negotiation Capability
                uint32_t contextSize : 1;      // Context size (1 = 64 bytes, 0 = 32 byte)
                uint32_t portPowerControl : 1; // Port power control capability
                uint32_t portIndicators : 1;   // Port indicator control capability
                uint32_t lightHCResetCap : 1;  // Capable of light host controller reset
                uint32_t latencyToleranceMessagingCapability : 1;
                uint32_t noSecondarySIDSupport : 1;
                uint32_t parseAllEventData : 1;
                uint32_t shortPacketCapability : 1;
                uint32_t stoppedEDTLACap : 1;
                uint32_t contiguousFrameIDCap : 1;
                uint32_t maxPSASize : 4;
                uint32_t extendedCapPointer : 16; // XHCI extended capabilities pointer
            } __attribute__((packed));
        };
        uint32_t dbOff;  // Doorbell offset
        uint32_t rtsOff; // Runtime registers space offset
        uint32_t hccParams2;

        inline uint8_t MaxSlots() { // Number of Device Slots
            return hcsParams1 & 0xFF;
        }

        inline uint16_t MaxIntrs() { // Number of Interrupters
            return (hcsParams1 >> 8) & 0x3FF;
        }

        inline uint8_t MaxPorts() { // Number of Ports
            return (hcsParams1 >> 24) & 0xFF;
        }

        inline uint8_t IST() { // Isochronous Scheduling Threshold
            return hcsParams2 & 0xF;
        }

        inline uint8_t ERSTMax() { // Event Ring Segment Table Max
            return (hcsParams2 >> 4) & 0xF;
        }

        inline uint16_t MaxScratchpadBuffers() {
            return (((hcsParams2 >> 21) & 0x1F) << 5) | ((hcsParams2 >> 27) & 0x1F);
        }

        inline uint8_t U1DeviceExitLatency() { return hcsParams3 & 0xFF; }

        inline uint16_t U2DeviceExitLatency() { return (hcsParams3 >> 16) & 0xFFFF; }

        inline uint16_t ExtendedCapabilitiesOffset() { return (extendedCapPointer << 2); }

        inline uint32_t DoorbellOffset() { return dbOff & (~0x1ULL); }
    } __attribute__((packed)) xhci_cap_regs_t; // Capability Registers

    typedef struct {
        uint32_t usbCommand; // USB Command
        uint32_t usbStatus;  // USB Status
        uint32_t pageSize;   // Page Size
        uint8_t rsvd1[8];
        uint32_t deviceNotificationControl; // Device Notification Control
        union {
            uint64_t cmdRingCtl; // Command Ring Control
            struct {
                uint64_t cmdRingCtlRCS : 1; // Ring cycle state
                uint64_t cmdRingCtlCS : 1;  // Command stop
                uint64_t cmdRingCtlCA : 1;  // Command Abort
                uint64_t cmdRingCtlCRR : 1; // Command Ring Running
                uint64_t cmdRingCtlReserved : 2;
                uint64_t cmdRingCtlPointer : 58;
            } __attribute__((packed));
        } __attribute__((packed));
        uint8_t rsvd2[16];
        uint64_t devContextBaseAddrArrayPtr; // Device Context Base Address Array Pointer
        uint32_t configure;                  // Configure

        inline void SetMaxSlotsEnabled(uint8_t value) {
            uint32_t temp = configure;

            temp &= ~((uint32_t)USB_CFG_MAXSLOTSEN);
            temp |= temp & USB_CFG_MAXSLOTSEN;

            configure = temp;
        }

        inline uint8_t MaxSlotsEnabled() { return configure & USB_CFG_MAXSLOTSEN; }
    } __attribute__((packed)) xhci_op_regs_t; // Operational Registers

    typedef struct {
        uint32_t portSC;             // Port Status and Control
        uint32_t portPMSC;           // Power Management Status and Control
        uint32_t portLinkInfo;       // Port Link Info
        uint32_t portHardwareLPMCtl; // Port Hardware LPM Control

        inline bool Powered() { return portSC & XHCI_PORTSC_PP; }

        inline void PowerOn() { portSC |= XHCI_PORTSC_PP; }

        inline void Reset() { portSC |= XHCI_PORTSC_PR; }

        inline void WarmReset() { portSC |= XHCI_PORTSC_WPR; }

        inline bool Enabled() { return portSC & XHCI_PORTSC_PED; }

        inline bool Connected() { return portSC & XHCI_PORTSC_CCS; }
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
        uint32_t speed : 4;        // Speed (deprecated)
        uint32_t resvd : 1;
        uint32_t mtt : 1;        // Multi TT
        uint32_t hub : 1;        // Hub (1) or Function (0)
        uint32_t ctxEntries : 5; // Index of the last valid Endpoint Context
        // Offset 04h
        uint32_t maxExitLatency : 16;
        uint32_t rootHubPortNumber : 8; // Root Hub Port Number
        uint32_t numberOfPorts : 8;     // If Hub then set to number of downstream facing ports
        // Offset 08h
        uint32_t parentHubSlotID : 8;
        uint32_t parentPortNumber : 8; // Parent Port Number
        uint32_t ttt : 2;              // TT Think Time
        uint32_t resvdZ : 4;
        uint32_t interrupterTarget : 10;
        // Offset 0Ch
        uint32_t usbDeviceAddress : 8; // Address assigned to USB device by the Host Controller
        uint32_t resvdZ_2 : 19;
        uint32_t slotState : 5;
    } __attribute__((packed)) xhci_slot_context_t;

    enum EndpointState {
        EndpointStateDisabled = 0,
        EndpointStateRunning = 1,
        EndpointStateHalted = 2,
        EndpointStateStopped = 3,
        EndpointStateError = 4,
    };

    enum EndpointType {
        EndpointTypeInvalid = 0,
        EndpointTypeIsochronousOut = 1,
        EndpointTypeBulkOut = 2,
        EndpointTypeInterruptOut = 3,
        EndpointTypeControl = 4, // Bidirectional
        EndpointTypeIsochronousIn = 5,
        EndpointTypeBulkIn = 6,
        EndpointTypeInterruptIn = 7,
    };

    typedef struct {
        uint32_t epState : 3; // Endpoint State (0 - Disabled, 1 - Running, 2 - Halted, 3 - Stopped, 4 - Error)
        uint32_t rsvdZ : 5;
        uint32_t mult : 2; // Maximum number of bursts within an interval that this endpoint supports (zero based),
                           // should be zero for anything not isochronous
        uint32_t maxPStreams : 5; // Maximum primary stream IDs
        uint32_t linearStreamArray : 1;
        uint32_t interval : 8; // The period between consecutive requests to a USB endpoint to send or receive data in
                               // 125 microsecond increments
        uint32_t maxESITPayloadHigh : 8;
        uint32_t rsvdZ2 : 1;
        uint32_t errorCount : 2;   // This field defines a 2-bit down count with the number of consecutive bus errors
                                   // allowed while executing a TD
        uint32_t endpointType : 3; // Endpoint type
        uint32_t rsvdZ3 : 1;
        uint32_t hostInitiateDisable : 1; //  Host Initiate Disable
        uint32_t maxBurstSize : 8;        // Maximum number of consecutive USB transactions that should be executed per
                                          // scheduling opportunity (zero based)
        uint32_t maxPacketSize : 16;
        uint64_t dequeueCycleState : 1;
        uint64_t rsvdZ4 : 3;
        uint64_t trDequeuePointer : 60;
        uint32_t averageTRBLength : 16;  // Average length of TRBs executed by this endpoint
        uint32_t maxESITPayloadLow : 16; // Max Endpoint Service Time Interval Payload Low
        uint32_t rsvdO[3];
    } __attribute__((packed)) xhci_endpoint_context_t;

    typedef struct {
        uint32_t interruptPending : 1;       // Interrupt Pending, if 1, an interrupt is pending
        uint32_t interruptEnable : 1;        // Interrupt Enable, if 1 the interrupter can generate interupts
        uint32_t rsvdP : 30;                 // Reserved
        uint32_t intModerationInterval : 16; // Interrupt Moderation Level (Default '4000' or 1ms) Default = ‘4000’
                                             // (~1ms). Minimum inter-interrupt interval specified in 250ns increments
        uint32_t intModerationCounter : 16; // Down counter loaded with the interrupt moderation interval when interrupt
                                            // pending is cleared to 0.
        uint32_t eventRingSegmentTableSize : 16; // Event ring segments supported
        uint32_t rsvdP2 : 16;
        uint64_t eventRingSegmentTableBaseAddress;
        union {
            uint64_t eventRingDequeuePointer;
            struct {
                uint32_t dequeueERSTSegmentIndex : 3;
                uint32_t eventHandlerBusy : 1; // 1 when interruptPending is set to 1 and 0 when the dequeue pointer is
                                               // written
                uint32_t eventRingDequeuePointerLow : 28;
                uint32_t eventRingDequeuePointerHigh;
            };
        };

        inline void SetEventRingSegmentTableAddress(uintptr_t addr) {
            assert(!(addr & 0xFFF));
            eventRingSegmentTableBaseAddress = addr;
        }

        inline void SetEventRingDequeuePointer(uintptr_t addr) {
            assert(!(addr & 0xFFF));
            eventRingDequeuePointer = addr | (eventRingDequeuePointer & 0xF);
        }
    } __attribute__((packed)) xhci_interrupter_t;

    typedef struct {
        union {
            uint64_t ringSegmentBaseAddress;
            struct {
                uint32_t rsvdZ : 6;
                uint32_t ringSegmentBaseAddressLo : 26;
                uint32_t ringSegmentBaseAddressHigh : 32;
            } __attribute__((packed));
        } __attribute__((packed));
        uint32_t ringSegmentSize : 16;
        uint32_t rsvdZ2 : 16;
        uint32_t rsvd3;
    } __attribute__((packed)) xhci_event_ring_segment_table_entry_t;
    static_assert(XHCI_EVENT_RING_SEGMENT_TABLE_ENTRY_SIZE == sizeof(xhci_event_ring_segment_table_entry_t));

    typedef struct {
        uint32_t microframeIndex;
        uint32_t rsvdZ[7]; // Reserved
        xhci_interrupter_t interrupters[];
    } __attribute__((packed)) xhci_runtime_regs_t;
    static_assert(offsetof(xhci_runtime_regs_t, interrupters) == 0x20);

    enum DoorbellTarget {
        DBTargetHostCommand = 0,
        DBTargetControlEndpointEnqueuePointer = 1,
        DBTargetEndpointOutEnqueuePointerBase = 2,
        DBTargetEndpointInEnqueuePointerBase = 3,
    };

    inline static uint8_t EndpointInDoorbellTarget(uint8_t endpoint) {
        return DBTargetEndpointInEnqueuePointerBase + (endpoint - 1);
    }

    inline static uint8_t EndpointOutDoorbellTarget(uint8_t endpoint) {
        return DBTargetEndpointOutEnqueuePointerBase + (endpoint - 1);
    }

    typedef union {
        uint64_t doorbell;
        struct {
            uint32_t target : 8; // 0 for host command ring
            uint32_t rsvdZ : 8;
            uint32_t streamID : 16; // Should be 0 for host controller doorbells
        } __attribute__((packed));
    } __attribute__((packed)) xhci_doorbell_register_t;

    typedef union {
        struct {
            uint32_t capability;
        };
        struct {
            uint32_t capID : 8;                   // Capability ID
            uint32_t nextCap : 8;                 // Next capability pointer
            uint32_t controllerBIOSSemaphore : 1; // When 1, BIOS owns the controller
            uint32_t rsvdP : 7;                   // Reserved
            uint32_t controllerOSSemaphore : 1;   // When 1 and BIOS Semaphore 0, OS owns the controller
            uint32_t rsvdP2 : 7;                  // Reserved
        } __attribute__((packed));
    } __attribute__((packed))
    xhci_ext_cap_legacy_support_t; // If present, should always be the first extended capability

    typedef struct {
        uint32_t capID : 8;          // Capability ID
        uint32_t nextCap : 8;        // Next capability pointer
        uint32_t minorRevision : 8;  // Minor revision (BCD, 0 for USB 3, 0x10 for USB 3.1)
        uint32_t majorRevision : 8;  // Major revision (BCD, 3 for USB 3.x, 2 for USB 2)
        uint8_t name[4];             // Protocol name, should be "USB "
        uint32_t portOffset : 8;     // Starting port number of Root Hub ports that support this protocol
        uint32_t portCount : 8;      // Number of consecutive ports that support this protocol
        uint32_t protoSpecific : 12; // Protocol specific
        uint32_t speedIDCount : 4;   // Number of Protocol Speed ID dwords that the structure contains
        uint32_t slotType : 4;       // Slot Type
        uint32_t rsvdP : 28;
    } __attribute__((packed)) xhci_ext_cap_supported_protocol_t;

    typedef struct {
        xhci_endpoint_context_t out;
        xhci_endpoint_context_t in;
    } __attribute__((packed)) xhci_endpoint_context_pair_t;

    typedef struct {
        xhci_slot_context_t slot;
        xhci_endpoint_context_t controlEndpoint;
        xhci_endpoint_context_pair_t endpoints[15];
    } __attribute__((packed)) xhci_device_context_t;

    enum TRBTypes {
        // Transfer ring only
        TRBTypeNormal = 1,
        TRBTypeSetup = 2,
        TRBTypeData = 3,
        TRBTypeStatus = 4,
        TRBTypeIsoch = 5,
        TRBTypeLink = 6, // Command and transfer ring
        TRBTypeEventData = 7,
        TRBTypeNoOp = 8,
        // Command ring only
        TRBTypeEnableSlotCommand = 9,
        TRBTypeDisableSlotCommand = 10,
        TRBTypeAddressDeviceCommand = 11,
        TRBTypeConfigureEndpointCommand = 12,
        TRBTypeEvaluateContextCommand = 13,
        TRBTypeResetEndpointCommand = 14,
        TRBTypeStopEndpointCommand = 15,
        TRBTypeSetTRDequeuePointerCommand = 16,
        TRBTypeResetDeviceCommand = 17,
        TRBTypeForceEventCommand = 18,                // Used with virtualization only
        TRBTypeNegotiateBandwidthCommand = 19,        // Optional
        TRBTypeSetLatencyZToleranceValueCommand = 20, // Optional
        TRBTypeGetPortBandwidthCommand = 21,          // Optional
        TRBTypeForceHeaderCommand = 22,
        TRBTypeNoOpCommand = 23,
        TRBTypeGetExtendedPropertyCommand = 24, // Optional
        TRBTypeSetExtendedPropertyCommand = 25, // Optional
        // Event ring only
        TRBTypeTransferEvent = 32,
        TRBTypeCommandCompletionEvent = 33,
        TRBTypePortStatusChangeEvent = 34,
        TRBTypeBandwidthRequestEvent = 35, // Optional
        TRBTypeDoorbellEvent = 36,         // Used with virtualization only
        TRBTypeHostControllerEvent = 37,
        TRBTypeDeviceNotificationEvent = 38,
        TRBTypeMFINDEXWrapEvent = 39,
    };

    enum TRBTransferTypes {
        NoDataStage = 0,
        Reserved = 1,
        OUTDataStage = 2,
        INDataStage = 3,
    };

    typedef struct {
        uint32_t parameter[2];
        uint32_t status;
        uint32_t cycleBit : 1;
        uint32_t extra : 9;
        uint32_t trbType : 6;
        uint32_t control : 16;
    } __attribute__((packed)) xhci_trb_t;

    typedef struct { // Generic event TRB
        uint64_t ptr;
        uint32_t rsvdZ : 24;
        uint32_t completionCode : 8;
        uint32_t cycleBit : 1;
        uint32_t rsvdZ2 : 9;
        uint32_t trbType : 6;
        uint32_t rsvdZ3 : 16;
    } __attribute__((packed)) xhci_event_trb_t;

    typedef struct {
        uint64_t segmentPtr;
        uint32_t rsvdZ : 24;
        uint32_t interrupterTarget : 8;
        uint32_t cycleBit : 1;
        uint32_t toggleCycle : 1;
        uint32_t rsvdZ2 : 2;
        uint32_t chainBit : 1;
        uint32_t interruptOnCompletion : 1;
        uint32_t rsvdZ3 : 4;
        uint32_t trbType : 6;
        uint32_t rsvdZ4 : 16;
    } __attribute__((packed)) xhci_link_trb_t;

    // One 'work item' is a Transfer Descriptor (TD) and contains multiple Transfer Request Blocks (TRBs)
    typedef struct {
        uint64_t dataBuffer;
        uint32_t transferLength : 17;    // Number of bytes the controller should send during the execution of the TRB
        uint32_t size : 5;               // Number of packets remaining in the TD
        uint32_t interrupterTarget : 10; // Index of the interrupter that will receive the event
        uint32_t cycleBit : 1;           // Marks the enqueue pointer of the transfer ring
        uint32_t evaluateNextTRB : 1;    // If 1, the controller will fetch and evaluate the next TRB before saving the
                                         // endpoint state
        uint32_t interruptOnShortPacket : 1;
        uint32_t noSnoop : 1;               // Some PCIe thing
        uint32_t chainBit : 1;              // When 1 this TRB is associated with the next, this is used to make a TD.
        uint32_t interruptOnCompletion : 1; // When this TRB completes send an interrupt
        uint32_t immediateData : 1; // If 1, the data buffer pointer contains data, not a pointer and the length should
                                    // be no more than 8
        uint32_t rsvdZ : 2;
        uint32_t blockEventInterrupt : 1; // If 1 and interruptOnCompletion is 1 then a transfer event will NOT generate
                                          // an interrupt
        uint32_t trbType : 6;
        uint32_t rsvdZ2 : 16;
    } __attribute__((packed)) xhci_normal_trb_t;

    typedef struct {
        uint32_t bmRequestType : 8;
        uint32_t bRequest : 8;
        uint32_t wValue : 16;
        uint32_t wIndex : 16;
        uint32_t wLength : 16;
        uint32_t trbTransferLength : 17; // Always should be 8
        uint32_t rsvdZ : 5;
        uint32_t interrupterTarget : 10; // Index of the interrupter that will receive the event
        uint32_t cycleBit : 1;           // Marks the enqueue pointer of the transfer ring
        uint32_t rsvdZ2 : 4;
        uint32_t interruptOnCompletion : 1; // When this TRB completes send an interrupt
        uint32_t immediateData : 1;         // Should be 1 for a setup TRB. If 1, the data buffer pointer contains data
        uint32_t rsvdZ3 : 3;
        uint32_t trbType : 6; // Shouuld be TRBTypeSetup
        uint32_t transferType : 2;
        uint32_t rsvdZ4 : 14;
    } __attribute__((packed)) xhci_setup_trb_t;

    typedef struct {
        uint64_t dataBuffer;
        uint32_t transferLength : 17;    // When OUT, number of bytes the controller will send. When IN, the size of the
                                         // data buffer referenced by the data buffer pointer
        uint32_t size : 5;               // Number of packets remaining in the TD
        uint32_t interrupterTarget : 10; // Index of the interrupter that will receive the event
        uint32_t cycleBit : 1;           // Marks the enqueue pointer of the transfer ring
        uint32_t evaluateNextTRB : 1;    // If 1, the controller will fetch and evaluate the next TRB before saving the
                                         // endpoint state
        uint32_t interruptOnShortPacket : 1;
        uint32_t noSnoop : 1;               // Some PCIe thing
        uint32_t chainBit : 1;              // When 1 this TRB is associated with the next, this is used to make a TD.
        uint32_t interruptOnCompletion : 1; // When this TRB completes send an interrupt
        uint32_t immediateData : 1; // If 1, the data buffer pointer contains data, not a pointer and the length should
                                    // be no more than 8
        uint32_t rsvdZ : 3;
        uint32_t trbType : 6;
        uint32_t direction : 1; // If 0 the transfer is OUT, if 1 the transfer direction is IN.
        uint32_t rsvdZ2 : 15;
    } __attribute__((packed)) xhci_data_trb_t;

    typedef struct {
        uint32_t rsvdZ0[2];
        uint32_t rsvdZ : 22;
        uint32_t interrupterTarget : 10; // Index of the interrupter that will receive the event
        uint32_t cycleBit : 1;           // Marks the enqueue pointer of the transfer ring
        uint32_t evaluateNextTRB : 1;    // If 1, the controller will fetch and evaluate the next TRB before saving the
                                         // endpoint state
        uint32_t rsvdZ2 : 2;
        uint32_t chainBit : 1;              // When 1 this TRB is associated with the next, this is used to make a TD.
        uint32_t interruptOnCompletion : 1; // When this TRB completes send an interrupt
        uint32_t rsvdZ3 : 4;
        uint32_t trbType : 6;
        uint32_t direction : 1; // If 0 the transfer is OUT, if 1 the transfer direction is IN.
        uint32_t rsvdZ4 : 15;
    } __attribute__((packed)) xhci_status_trb_t;

    typedef struct {
        uint32_t rsvdZ[2];
        uint32_t rsvdZ2 : 22;
        uint32_t interrupterTarget : 10; // Index of the interrupter that will receive the event
        uint32_t cycleBit : 1;           // Marks the enqueue pointer of the transfer ring
        uint32_t evaluateNextTRB : 1;    // If 1, the controller will fetch and evaluate the next TRB before saving the
                                         // endpoint state
        uint32_t rsvdZ3 : 2;
        uint32_t chainBit : 1;              // When 1 this TRB is associated with the next, this is used to make a TD.
        uint32_t interruptOnCompletion : 1; // When this TRB completes send an interrupt
        uint32_t rsvdZ4 : 4;
        uint32_t trbType : 6;
        uint32_t rsvdZ5 : 16;
    } __attribute__((packed)) xhci_noop_trb_t;

    typedef struct {
        uint64_t pointer;             // Address of the TRB that generated this event or event data
        uint32_t transferLength : 24; // Number of bytes not tranferred
        uint32_t completionCode : 8;
        uint32_t cycleBit : 1; // Marks the Dequeue pointer of the event ring
        uint32_t rsvdZ : 1;
        uint32_t eventData : 1; // If 1, the pointer contains devent data, otherwise it contains a pointer to the TRB
                                // that generated the event
        uint32_t rsvdZ2 : 7;
        uint32_t trbType : 6;
        uint32_t endpointID : 5; // ID the endpoint that generated the event
        uint32_t rsvdZ3 : 3;
        uint32_t slotID : 8; // ID of the Device Slot that generated the event
    } __attribute__((packed)) xhci_transfer_event_trb_t;

    typedef struct {
        uint64_t commandTRBPointer;
        uint32_t commandCompletionParameter : 24; // Command dependant
        uint32_t completionCode : 8;
        uint32_t cycleBit : 1; // Marks the Dequeue pointer of the event ring
        uint32_t rsvdZ : 9;
        uint32_t trbType : 6;
        uint32_t vfID : 8;   // ID of the virtual function that generated the event
        uint32_t slotID : 8; // ID of the Device Slot that generated the event
    } __attribute__((packed)) xhci_command_completion_event_trb_t;

    typedef struct {
        uint32_t rsvdZ[3];
        uint32_t cycleBit : 1; // Marks the Enqueue pointer of the command ring
        uint32_t rsvdZ2 : 9;
        uint32_t trbType : 6;
        uint32_t rsvdZ3 : 16;
    } __attribute__((packed)) xhci_noop_command_trb_t;

    typedef struct {
        uint32_t rsvdZ[3];
        uint32_t cycleBit : 1; // Marks the Enqueue pointer of the command ring
        uint32_t rsvdZ2 : 9;
        uint32_t trbType : 6;
        uint32_t slotType : 5; // The type of slot that will be enabled
        uint32_t rsvdZ3 : 11;
    } __attribute__((packed)) xhci_enable_slot_command_trb_t;

    typedef struct {
        uint32_t rsvdZ[3];
        uint32_t cycleBit : 1; // Marks the Enqueue pointer of the command ring
        uint32_t rsvdZ2 : 9;
        uint32_t trbType : 6;
        uint32_t rsvdZ3 : 8;
        uint32_t slotID : 8; // The slot to disable
    } __attribute__((packed)) xhci_disable_slot_command_trb_t;

    typedef struct {
        uint64_t inputContextPointer;
        uint32_t rsvdZ;
        uint32_t cycleBit : 1; // Marks the Enqueue pointer of the command ring
        uint32_t rsvdZ2 : 8;
        uint32_t blockSetAddressRequest : 1; // When 1 the command will not generate a USB SET_ADDRESS request
        uint32_t trbType : 6;
        uint32_t rsvdZ3 : 8;
        uint32_t slotID : 8; // The slot to disable
    } __attribute__((packed)) xhci_address_device_command_trb_t;

    typedef struct {
        uint64_t inputContextPointer;
        uint32_t rsvdZ;
        uint32_t cycleBit : 1; // Marks the Enqueue pointer of the command ring
        uint32_t rsvdZ2 : 8;
        uint32_t deconfigure : 1; // When 1 the command will "deconfigure" the Device Slot.
        uint32_t trbType : 6;
        uint32_t rsvdZ3 : 16;
    } __attribute__((packed)) xhci_configure_endpoint_command_trb_t;

    static_assert(XHCI_TRB_SIZE == sizeof(xhci_trb_t));

    static_assert(XHCI_TRB_SIZE == sizeof(xhci_event_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_link_trb_t));

    static_assert(XHCI_TRB_SIZE == sizeof(xhci_configure_endpoint_command_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_address_device_command_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_disable_slot_command_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_enable_slot_command_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_noop_command_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_command_completion_event_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_transfer_event_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_noop_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_status_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_data_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_setup_trb_t));
    static_assert(XHCI_TRB_SIZE == sizeof(xhci_normal_trb_t));

    struct Port {
        xhci_ext_cap_supported_protocol_t* protocol;
        xhci_port_regs_t* registers;
    };

    struct Device {
        XHCIController* c;

        int port = -1;
        int slot = -1;

        void AllocateSlot();

        ALWAYS_INLINE Device(XHCIController* c) : c(c) {}
    };

    struct CommandCompletionEvent {
        Semaphore completion = Semaphore(0);
        union {
            xhci_command_completion_event_trb_t event;
            xhci_event_trb_t rawEvent;
        };
    };

    struct XHCIEventRingSegment {
        xhci_event_ring_segment_table_entry_t* entry; // Entry given to the xHC

        uint64_t segmentPhys;      // The physical address of the segment itself
        xhci_event_trb_t* segment; // The segment itself

        size_t size; // Segment size in TRB entries
    };

    uintptr_t xhciBaseAddress;
    uintptr_t xhciVirtualAddress;

    xhci_cap_regs_t* capRegs;
    xhci_op_regs_t* opRegs;
    xhci_port_regs_t* portRegs;
    xhci_runtime_regs_t* runtimeRegs;

    xhci_doorbell_register_t* doorbellRegs;

    xhci_interrupter_t* interrupter;
    uintptr_t eventRingSegmentTablePhys = 0;
    xhci_event_ring_segment_table_entry_t* eventRingSegmentTable = nullptr;

    unsigned eventRingSegmentTableSize = 0;
    unsigned eventRingDequeue = 0;
    XHCIEventRingSegment* eventRingSegments = nullptr;
    bool eventRingCycleState = true; // Software maintains an Event Ring Consumer Cycle State (CCS) bit, initializing it
                                     // to ‘1’ and toggling it every time the Event Ring Dequeue Pointer wraps back to
                                     // the beginning of the Event Ring. If the Cycle bit of the Event TRB pointed to by
                                     // the Event Ring Dequeue Pointer equals CCS, then the Event TRB is a valid event.

    uintptr_t devContextBaseAddressArrayPhys = 0;
    uint64_t* devContextBaseAddressArray = nullptr;

    struct CommandRing {
        XHCIController* hcd;

        uintptr_t physicalAddr;
        xhci_trb_t* ring = nullptr;
        unsigned enqueueIndex = 0;
        unsigned maxIndex = 0;
        bool cycleState = 1;

        lock_t lock;
        CommandCompletionEvent* events = nullptr;

        ALWAYS_INLINE CommandRing(XHCIController* c) : hcd(c) {}

        void SendCommandRaw(void* cmd);
        xhci_command_completion_event_trb_t SendCommand(void* cmd);
    } commandRing = CommandRing(this);

    uintptr_t scratchpadBuffersPhys = 0;
    uint64_t* scratchpadBuffers = nullptr;

    uintptr_t extCapabilities;

    uint8_t maxSlots;

    uint8_t controllerIRQ = 0xFF;
    Vector<xhci_ext_cap_supported_protocol_t*> protocols;

    Port* ports;
    List<Device*> m_devices;

    bool TakeOwnership();

    void InitializeProtocols();
    void InitializePorts();

    void IRQHandler(RegisterContext* r);

    void EnableSlot();

    void OnInterrupt();

    Status controllerStatus = ControllerNotInitialized;
};
} // namespace USB
