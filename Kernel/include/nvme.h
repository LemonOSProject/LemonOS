#pragma once

#include <stdint.h>

#include <pci.h>
#include <device.h>
#include <assert.h>

#define NVME_CAP_CMBS (1 << 57) // Controller memory buffer supported
#define NVME_CAP_PMRS (1 << 56) // Persistent memory region supported
#define NVME_CAP_BPS (1 << 45) // Boot partition support
#define NVME_CAP_NVM_CMD_SET (1 << 37) // NVM command set supported
#define NVME_CAP_NSSRS (1 << 36) // NVM subsystem reset supported
#define NVME_CAP_CQR (1 << 16) // Contiguous Queues Required

#define NVME_CAP_MPS_MASK 0xfU
#define NVME_CAP_MPSMAX(x) ((x >> 52) & NVME_CAP_MPS_MASK) // Max supported memory page size (2 ^ (12 + MPSMAX))
#define NVME_CAP_MPSMIN(x) ((x >> 48) & NVME_CAP_MPS_MASK) // Min supported memory page size (2 ^ (12 + MPSMIN))

#define NVME_CAP_DSTRD_MASK 0xfU
#define NVME_CAP_DSTRD(x) (((x) >> 32) & NVME_CAP_DSTRD_MASK) // Doorbell stride (2 ^ (2 + DSTRD)) bytes

#define NVME_CAP_MQES_MASK 0xffffU
#define NVME_CAP_MQES(x) ((x) & NVME_CAP_MQES_MASK) // Maximum queue entries supported

#define NVME_CFG_MPS_MASK 0xfUL
#define NVME_CFG_MPS(x) (((x) & NVME_CFG_MPS_MASK) << 7) // Host memory page size (2 ^ (12 + MPSMIN))
#define NVME_CFG_CSS_MASK 0b111U // Command set selected
#define NVME_CFG_CSS(x) (((x) & NVME_CFG_CSS_MASK) << 4)
#define NVME_CFG_ENABLE (1 << 0)

#define NVME_CFG_DEFAULT_IOCQES (4 << 20) // 16 bytes so log2(16) = 4
#define NVME_CFG_DEFAULT_IOSQES (6 << 16) // 64 bytes so log2(64) = 6

#define NVME_CSTS_FATAL (1 << 1)
#define NVME_CSTS_READY (1 << 0) // Set to 1 when the controller is ready to accept submission queue doorbell writes

#define NVME_AQA_AQS_MASK 0xfffU // Admin queue size mask
#define NVME_AQA_ACQS(x) (((x) & NVME_AQA_AQS_MASK) << 16) // Admin completion queue size
#define NVME_AQA_ASQS(x) (((x) & NVME_AQA_AQS_MASK) << 0) // Admin submission queue size

#define NVME_NSSR_RESET_VALUE 0x4E564D65h // "NVME", initiates a reset

namespace NVMe{
	struct NVMeIdentifyCommand{
		enum{
			CNSNamespace = 0,
			CNSController = 1,
			CNSNamespaceList = 2,
		};

		struct{
			uint32_t cns : 8; // Controller or Namespace structure
			uint32_t reserved : 8; // Reserved
			uint32_t cntID : 16; // Controller Identifier
		} __attribute__((packed)); // DWORD 10
		uint32_t nvmSetID; // DWORD 11 - NVM set identifier
	};

	struct NVMeCreateIOCompletionQueueCommand{
		struct{
			uint32_t queueID : 16; // ID corresponding to the completion queue head doorbell
			uint32_t queueSize : 16; // Size of completion queue, 0's based value
		} __attribute__((packed)); // DWORD 10
		struct {
			uint32_t contiguous : 1; // If 1 then the queue is physically contiguous and PRP 1 is the address of a contiguous buffer
			uint32_t intEnable : 1; // Enable interrupts for the completion queue
			uint32_t reserved : 14;
			uint32_t intVector : 16; // Interrupt vector, 0h for single message MSI
		} __attribute__((packed)); // DWORD 11
	};

	struct NVMeCreateIOSubmissionQueueCommand{
		struct{
			uint32_t queueID : 16; // ID corresponding to the submission queue head doorbell
			uint32_t queueSize : 16; // Size of submission queue, 0's based value
		} __attribute__((packed)); // DWORD 10
		struct {
			uint32_t contiguous : 1; // If 1 then the queue is physically contiguous and PRP 1 is the address of a contiguous buffer
			uint32_t priority : 2; // Queue priority
			uint32_t reserved : 13;
			uint32_t cqID : 16; // Completion queue ID
		} __attribute__((packed)); // DWORD 11
	};

	struct NVMeDeleteIOQueueCommand{
		uint32_t queueID;
	};

	struct NVMeReadCommand{
		uint64_t startLBA; // DWORD 10 - 11, First logical block to be read from
		struct{
			uint32_t blockNum : 16; // Number of logical blocks to be read;
			uint32_t reserved : 10;
			uint32_t prInfo : 4; // Protection information field
			uint32_t forceUnitAccess : 1; // Force unit access
			uint32_t limitedRetry : 1; // Limited retry
		} __attribute__((packed)); // DWORD 12
	};

	struct NVMeWriteCommand{
		uint64_t startLBA; // DWORD 10 - 11, First logical block to be written
		struct{
			uint32_t blockNum : 16; // Number of logical blocks to be written;
			uint32_t reserved2 : 4;
			uint32_t directiveType : 4;
			uint32_t reserved : 2;
			uint32_t prInfo : 4; // Protection information field
			uint32_t forceUnitAccess : 1; // Force unit access
			uint32_t limitedRetry : 1; // Limited retry
		} __attribute__((packed)); // DWORD 12
	};

	struct NVMeCommand{
		struct{
			uint32_t opcode : 8; // Opcode
			uint32_t fuse : 2; // Fused operation
			uint32_t reserved : 4;
			uint32_t psdt : 2; // PRP or SGL selection, 0 for PRPs
			uint32_t commandID : 16; // Command identifier
		}__attribute__((packed)); // DWORD 0
		uint32_t nsID; // DWORD 1, Namespace identifier (0xFFFFFFFFh = broadcast)
		uint64_t reserved2;
		uint64_t metadataPtr; // DWORD 4 - 5, Metadata Pointer
		uint64_t prp1; // DWORD 6 - 7, PRP Entry 1
		uint64_t prp2; // DWORD 8 - 9, PRP Entry 2
		union{
			struct{
				uint32_t cmdDwords[6]; // DWORD 10 - 15, Extra command dword
			};
			NVMeIdentifyCommand identify;
			NVMeCreateIOCompletionQueueCommand createIOCQ;
			NVMeCreateIOSubmissionQueueCommand createIOSQ;
			NVMeDeleteIOQueueCommand deleteIOQ;
		};
	};
	static_assert(sizeof(NVMeCommand) == 64);

	struct NVMeCompletion{
		uint32_t dw0; // DWORD 0, Command specific
		uint32_t reserved; // DWORD 1, Reserved
		struct{
			uint32_t sqHead : 16; // DWORD 2, Submission queue (indicated in sqID) head pointer
			uint32_t sqID : 16; // DWORD 2, Submission queue ID
		} __attribute__((packed));
		struct{
			uint32_t commandID : 16; // Id of the command being completed
			uint32_t phaseTag : 1; // DWORD 3, Changed when a completion queue entry is new
			uint32_t status : 15; // DWORD 3, Status of command being completed
		} __attribute__((packed));
	};
	static_assert(sizeof(NVMeCompletion) == 16);

	class NVMeQueue{
		uint16_t queueID;

		uintptr_t completionBase;
		uintptr_t submissionBase;

		NVMeCompletion* completionQueue;
		NVMeCommand* submissionQueue;

		volatile uint32_t* completionDB;
		volatile uint32_t* submissionDB;

		uint16_t cQueueSize = 0; // Queue sizes in bytes
		uint16_t sQueueSize = 0; // Queue sizes in bytes

		uint16_t cqCount = 0; // Amount of elements in CQ
		uint16_t sqCount = 0; // Amount of elements in SQ

		lock_t queueLock = 0;

		uint16_t nextCommandID = 0;
	public:
		bool completionCycleState = true;
		uint16_t cqHead = 0;
		uint16_t sqTail = 0;

		///////////////////////////////
		/// \name NVMeQueue
		///
		/// \param cid Completion Queue ID
		/// \param sid Submission Queue ID
		/// \param cqBase Completion Queue physical base
		/// \param sqBase Submission Queue physical base
		/// \param cq Completion Queue virtual base
		/// \param sq Submission Queue virtual base
		/// \param sz Queue size in bytes
		///////////////////////////////
		NVMeQueue(uint16_t qid, uintptr_t cqBase, uintptr_t sqBase, void* cq, void* sq, uint32_t* cqDB, uint32_t* sqDB, uint16_t csz, uint16_t ssz);
		NVMeQueue() = default;

		long Consume(NVMeCommand& cmd);

		void Submit(NVMeCommand& cmd);
		void SubmitWait(NVMeCommand& cmd, NVMeCompletion& complet);

		__attribute__((always_inline)) uint16_t CQSize() { return cqCount; }
		__attribute__((always_inline)) uint16_t SQSize() { return sqCount; }
		__attribute__((always_inline)) uintptr_t CQBase() { return completionBase; }
		__attribute__((always_inline)) uintptr_t SQBase() { return submissionBase; }
	};

	class Controller{
	public:
		enum DriverStatus{
			ControllerNotReady,
			ControllerError,
			ControllerReady,
		};
	private:

		enum ControllerConfigCommandSet{
			NVMeConfigCmdSetNVM = 0,
			NVMeConfigCmdSetAdminOnly = 0b111,
		};

		enum AdminCommands{
			AdminCmdDeleteIOSubmissionQueue = 0x0,
			AdminCmdCreateIOSubmissionQueue = 0x1,
			AdminCmdGetLogPage              = 0x2,	
			AdminCmdDeleteIOCompletionQueue = 0x4,
			AdminCmdCreateIOCompletionQueue = 0x5,
			AdminCmdIdentify				= 0x6,
		};

		enum ControllerType{
			IOController = 1,
			DiscoveryController = 2,
			AdminController = 3,
		};

		struct ControllerIdentity{
			uint16_t vendorID; // PCI Vendor ID
			uint16_t subsystemVendorID; // PCI Subsystem Vendor ID
			int8_t serialNumber[20]; // ASCII encoded serial number
			int8_t modelNumber[40]; // ASCII encoded model number
			int8_t firmwareRevision[8]; // ASCII encoded firmware revision
			uint8_t recommendedArbitrationBurst;
			uint8_t ieee[3]; // IEEE Organization Unique Identifier
			uint8_t cmic; // Controller Multi-Path I/O and Namespace Sharing Capabilities
			uint8_t maximumDataTransferSize; // Maximum Data Transfer Size (2 ^ (CAP.MPSMIN * n))
			uint16_t controllerID; // NVM unique controller ID
			uint32_t version;
			uint32_t rtd3ResumeLatency;
			uint32_t rtd3EntryLatency;
			uint32_t oaes;
			uint32_t controllerAttributes;
			uint16_t rrls;
			uint8_t reserved[9];
			uint8_t controllerType;
			uint8_t fGUID[16];
			uint16_t crdt[3];
			uint8_t reserved2[122];
			uint16_t oacs;
			uint8_t acl;
			uint8_t aerl;
			uint8_t firmwareUpdates;
			uint8_t logPageAttributes;
			uint8_t errorLogPageEntries;
			uint8_t numberOfPowerStates;
			uint8_t apsta;
			uint16_t wcTemp; // Minimum temperature to report overheat
			uint16_t ccTemp; // Critical temperature threshold
			uint16_t mtfa;
			uint32_t hostMemoryBufferPreferredSize; // In 4KB Units
			uint32_t hostMemoryBufferMinimumSize; // In 4KB Units
			uint8_t unused[232]; // Our driver does not care
			uint8_t sqEntrySize; // Submission Queue Entry Size (Bits 3:0) (2 ^ n)
			uint8_t cqEntrySize; // Completion Queue Entry Size  (Bits 3:0) (2 ^ n)
			uint16_t maxCmd;
			uint32_t numNamespaces; // Number of namespaces;
			uint8_t unused2[248];
			int8_t name[256]; // UTF-8 encoded NVMe Name (null-terminated)
			uint8_t unused3[3072];
		};
		static_assert(sizeof(ControllerIdentity) == 4096);

		struct NamespaceIdentity{
			uint64_t namespaceSize; // Namespace consists of LBA 0 to (n - 1)
			uint64_t nsCap; // Namespace capacity, indicates the maximum number of logical blocks that may be allocated in the namespace
			uint64_t nsUse; // Namespace utilization, indicates the current number of logical blocks allocated in the namespace
			uint8_t nsFeat; // Namespace features
			uint8_t numLBAFormats; // Number of LBA formats
			uint8_t unused[358];
			uint8_t vendor[3712]; // Vendor specific
		};
		static_assert(sizeof(NamespaceIdentity) == 4096);

		enum NVMCommands{
			NVMCmdFlush	 = 0x0,
			NVMCmdWrite	 = 0x1,
			NVMCmdRead	 = 0x2,
		};

		struct Registers{
			uint64_t cap; // Capabilities
			uint32_t version; // Version
			uint32_t intMask; // Interrupt Mask Set
			uint32_t intMaskClear; // Interrupt Mask Clear
			uint32_t config; // Controller Configuration
			uint32_t reserved;
			uint32_t status; // Controller status
			uint32_t nvmSubsystemReset; // NVM subsystem reset
			uint32_t adminQAttr; // Admin queue attributes
			uint64_t adminSubmissionQ; // Admin submission queue
			uint64_t adminCompletionQ; // Admin completion queue
			uint32_t controllerMemBufferLocation; // Controller memory buffer location (Optional)
			uint32_t controllerMemBufferSize; // Controller memory buffer size (Optional)
			uint32_t bootPartitionInfo; // Boot partition information (Optional)
			uint32_t bootPartitionReadSelect; // Boot partition read select (Optional)
			uint64_t bootPartitionMemBufferLocation; // Boot partition memory buffer location (Optional)
			uint64_t bootPartitionMemSpaceControl; // Boot partition memory space control (Optional)
			uint64_t controllerMemBufferMemSpaceControl; // Controller memory buffer memory space control (Optional)
			uint32_t controllerMemoryBufferStatus; // Controller memory buffer status (Optional)
		} __attribute__((packed));

		Registers* cRegs = nullptr;
		PCIDevice* pciDevice;
		
		ControllerIdentity* controllerIdentity = nullptr;
		uintptr_t controllerIdentityPhys = 0;

		uint16_t nextQueueID = 1;

		NVMeQueue adminQueue;

		DriverStatus dStatus;

		// Capabilities
		__attribute__((always_inline)) inline uint32_t GetMaxMemoryPageSize(){
			return 0x1000 /*(1 << 12)*/ << NVME_CAP_MPSMAX(cRegs->cap);
		}

		__attribute__((always_inline)) inline uint32_t GetMinMemoryPageSize(){
			return 0x1000 /*(1 << 12)*/ << NVME_CAP_MPSMIN(cRegs->cap);
		}

		__attribute__((always_inline)) inline uint32_t GetDoorbellStride(){
			return NVME_CAP_DSTRD(cRegs->cap);
		}

		__attribute__((always_inline)) inline uint16_t GetMaxQueueEntries(){
			uint16_t value = NVME_CAP_MQES(cRegs->cap);

			if(value <= 0){
				return UINT16_MAX;
			} else {
				return value + 1;
			}
		}

		// Config
		__attribute__((always_inline)) inline void SetMemoryPageSize(uint32_t size){
			assert(size >= GetMinMemoryPageSize() && size <= GetMaxMemoryPageSize());

			uint32_t i = 0;
			while(!(size & 1)) {
				i++;
				size >>= 1;
			}

			assert(i >= 12);
			cRegs->config = (cRegs->config & ~NVME_CFG_MPS(NVME_CFG_MPS_MASK)) | NVME_CFG_MPS(i - 12); // Host memory page size (2 ^ (12 + MPSMIN))
		}

		__attribute__((always_inline)) inline void SetCommandSet(uint8_t set){
			cRegs->config = (cRegs->config & ~NVME_CFG_CSS(NVME_CFG_CSS_MASK)) | NVME_CFG_CSS(set);
		}

		__attribute__((always_inline)) inline void Enable(){
			cRegs->config |= NVME_CFG_ENABLE;
		}

		__attribute__((always_inline)) inline void Disable(){
			cRegs->config &= ~NVME_CFG_ENABLE;
		}

		__attribute__((always_inline)) inline void SetAdminCompletionQueueSize(uint16_t sz){
			cRegs->adminQAttr = (cRegs->adminQAttr & ~NVME_AQA_ACQS(NVME_AQA_AQS_MASK)) | NVME_AQA_ACQS(sz - 1); // 0's based
		}

		__attribute__((always_inline)) inline void SetAdminSubmissionQueueSize(uint16_t sz){
			cRegs->adminQAttr = (cRegs->adminQAttr & ~NVME_AQA_ASQS(NVME_AQA_AQS_MASK)) | NVME_AQA_ASQS(sz - 1); // 0's based
		}

		uint32_t* GetSubmissionDoorbell(uint16_t qID){
			assert(cRegs);
			return reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(cRegs) + 0x1000 + (2 * qID) * (4 << GetDoorbellStride()));
		}

		uint32_t* GetCompletionDoorbell(uint16_t qID){
			assert(cRegs);
			return reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(cRegs) + 0x1000 + (2 * qID + 1) * (4 << GetDoorbellStride()));
		}

		uint16_t AllocateQueueID() { return nextQueueID++; }
	public:
		Controller(PCIDevice* pciDev);
	
		long IdentifyController();

		__attribute__((always_inline)) inline DriverStatus Status(){ return dStatus; }
	};

	class Device : public DiskDevice{
		Controller* controller;

	public:
		Device(Controller* c){ controller = c; }
	};

	void Initialize();
}