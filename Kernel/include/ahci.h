#pragma once

#include <stdint.h>

enum
{
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
};

enum{
	SCTL_PORT_DET_INIT 	 = 0x1,
	SCTL_PORT_IPM_NOPART = 0x100, // No partial state
	SCTL_PORT_IPM_NOSLUM = 0x200, // No slumber state
	SCTL_PORT_IPM_NODSLP = 0x400, // No devslp state
};

typedef struct tagFIS_REG_H2D
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_REG_H2D
 
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:3;		// Reserved
	uint8_t  c:1;		// 1: Command, 0: Control
 
	uint8_t  command;	// Command register
	uint8_t  featurel;	// Feature register, 7:0
 
	// DWORD 1
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register
 
	// DWORD 2
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  featureh;	// Feature register, 15:8
 
	// DWORD 3
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  icc;		// Isochronous command completion
	uint8_t  control;	// Control register
 
	// DWORD 4
	uint8_t  rsv1[4];	// Reserved
} __attribute__((packed)) fis_reg_h2d_t;

typedef struct tagFIS_REG_D2H
{
	// DWORD 0
	uint8_t  fis_type;    // FIS_TYPE_REG_D2H
 
	uint8_t  pmport:4;    // Port multiplier
	uint8_t  rsv0:2;      // Reserved
	uint8_t  i:1;         // Interrupt bit
	uint8_t  rsv1:1;      // Reserved
 
	uint8_t  status;      // Status register
	uint8_t  error;       // Error register
 
	// DWORD 1
	uint8_t  lba0;        // LBA low register, 7:0
	uint8_t  lba1;        // LBA mid register, 15:8
	uint8_t  lba2;        // LBA high register, 23:16
	uint8_t  device;      // Device register
 
	// DWORD 2
	uint8_t  lba3;        // LBA register, 31:24
	uint8_t  lba4;        // LBA register, 39:32
	uint8_t  lba5;        // LBA register, 47:40
	uint8_t  rsv2;        // Reserved
 
	// DWORD 3
	uint8_t  countl;      // Count register, 7:0
	uint8_t  counth;      // Count register, 15:8
	uint8_t  rsv3[2];     // Reserved
 
	// DWORD 4
	uint8_t  rsv4[4];     // Reserved
} __attribute__((packed)) fis_reg_d2h_t;

typedef struct tagFIS_DATA
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_DATA
 
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:4;		// Reserved
 
	uint8_t  rsv1[2];	// Reserved
 
	// DWORD 1 ~ N
	uint32_t data[1];	// Payload
} __attribute__((packed)) fis_data;

typedef struct tagFIS_PIO_SETUP
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_PIO_SETUP
 
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		// Data transfer direction, 1 - device to host
	uint8_t  i:1;		// Interrupt bit
	uint8_t  rsv1:1;
 
	uint8_t  status;		// Status register
	uint8_t  error;		// Error register
 
	// DWORD 1
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register
 
	// DWORD 2
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  rsv2;		// Reserved
 
	// DWORD 3
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  rsv3;		// Reserved
	uint8_t  e_status;	// New value of status register
 
	// DWORD 4
	uint16_t tc;		// Transfer count
	uint8_t  rsv4[2];	// Reserved
} __attribute__((packed)) fis_pio_setup_t;

typedef struct tagFIS_DMA_SETUP
{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_DMA_SETUP
 
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		// Data transfer direction, 1 - device to host
	uint8_t  i:1;		// Interrupt bit
	uint8_t  a:1;            // Auto-activate. Specifies if DMA Activate FIS is needed
 
    uint8_t  rsved[2];       // Reserved
 
	//DWORD 1&2
	uint64_t DMAbufferID;    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory. SATA Spec says host specific and not in Spec. Trying AHCI spec might work.

	//DWORD 3
	uint32_t rsvd;           //More reserved

	//DWORD 4
	uint32_t DMAbufOffset;   //Byte offset into buffer. First 2 bits must be 0

	//DWORD 5
	uint32_t TransferCount;  //Number of bytes to transfer. Bit 0 must be 0

	//DWORD 6
	uint32_t resvd;          //Reserved
 
} __attribute__((packed)) fis_dma_setup_t;

typedef volatile struct tagHBA_PORT
{
	uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
	uint32_t clbu;		// 0x04, command list base address upper 32 bits
	uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
	uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
	uint32_t is;		// 0x10, interrupt status
	uint32_t ie;		// 0x14, interrupt enable
	uint32_t cmd;		// 0x18, command and status
	uint32_t rsv0;		// 0x1C, Reserved
	uint32_t tfd;		// 0x20, task file data
	uint32_t sig;		// 0x24, signature
	uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
	uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
	uint32_t serr;		// 0x30, SATA error (SCR1:SError)
	uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
	uint32_t ci;		// 0x38, command issue
	uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
	uint32_t fbs;		// 0x40, FIS-based switch control
	uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
	uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} __attribute__((packed)) hba_port_t;

typedef volatile struct tagHBA_MEM
{
	// 0x00 - 0x2B, Generic Host Control
	uint32_t cap;		// 0x00, Host capability
	uint32_t ghc;		// 0x04, Global host control
	uint32_t is;		// 0x08, Interrupt status
	uint32_t pi;		// 0x0C, Port implemented
	uint32_t vs;		// 0x10, Version
	uint32_t ccc_ctl;	// 0x14, Command completion coalescing control
	uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
	uint32_t em_loc;		// 0x1C, Enclosure management location
	uint32_t em_ctl;		// 0x20, Enclosure management control
	uint32_t cap2;		// 0x24, Host capabilities extended
	uint32_t bohc;		// 0x28, BIOS/OS handoff control and status
 
	// 0x2C - 0x9F, Reserved
	uint8_t  rsv[0xA0-0x2C];
 
	// 0xA0 - 0xFF, Vendor specific registers
	uint8_t  vendor[0x100-0xA0];
 
	// 0x100 - 0x10FF, Port control registers
	hba_port_t	ports[32];	// 1 ~ 32
} __attribute__((packed)) hba_mem_t;
 
typedef volatile struct tagHBA_FIS
{
	// 0x00
	fis_dma_setup_t	dsfis;		// DMA Setup FIS
	uint8_t         pad0[4];
 
	// 0x20
	fis_pio_setup_t	psfis;		// PIO Setup FIS
	uint8_t         pad1[12];
 
	// 0x40
	fis_reg_d2h_t	rfis;		// Register – Device to Host FIS
	uint8_t         pad2[4];
 
	// 0x58
	uint8_t	sdbfis[8];		// Set Device Bit FIS
 
	// 0x60
	uint8_t         ufis[64];
 
	// 0xA0
	uint8_t   	rsv[0x100-0xA0];
} __attribute__((packed)) hba_fis_t;

typedef struct tagHBA_CMD_HEADER
{
	// DW0
	uint8_t  cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
	uint8_t  a:1;		// ATAPI
	uint8_t  w:1;		// Write, 1: H2D, 0: D2H
	uint8_t  p:1;		// Prefetchable
 
	uint8_t  r:1;		// Reset
	uint8_t  b:1;		// BIST
	uint8_t  c:1;		// Clear busy upon R_OK
	uint8_t  rsv0:1;		// Reserved
	uint8_t  pmp:4;		// Port multiplier port
 
	uint16_t prdtl;		// Physical region descriptor table length in entries
 
	// DW1
	volatile
	uint32_t prdbc;		// Physical region descriptor byte count transferred
 
	// DW2, 3
	uint32_t ctba;		// Command table descriptor base address
	uint32_t ctbau;		// Command table descriptor base address upper 32 bits
 
	// DW4 - 7
	uint32_t rsv1[4];	// Reserved
} __attribute__((packed)) hba_cmd_header_t;
 
typedef struct tagHBA_PRDT_ENTRY
{
	uint32_t dba;		// Data base address
	uint32_t dbau;		// Data base address upper 32 bits
	uint32_t rsv0;		// Reserved
 
	// DW3
	uint32_t dbc:22;		// Byte count, 4M max
	uint32_t rsv1:9;		// Reserved
	uint32_t i:1;		// Interrupt on completion
} __attribute__((packed)) hba_prdt_entry_t;

typedef struct tagHBA_CMD_TBL
{
	// 0x00
	uint8_t  cfis[64];	// Command FIS
 
	// 0x40
	uint8_t  acmd[16];	// ATAPI command, 12 or 16 bytes
 
	// 0x50
	uint8_t  rsv[48];	// Reserved
 
	// 0x80
	hba_prdt_entry_t	prdt_entry[1];	// Physical region descriptor table entries, 0 ~ 65535
} __attribute__((packed)) hba_cmd_tbl_t;

#define AHCI_GHC_ENABLE (1 << 31)

#define AHCI_CAP_S64A (1 << 31) // 64-bit addressing
#define AHCI_CAP_NCQ (1 << 30) // Support for Native Command Queueing?
#define AHCI_CAP_SSS (1 << 27) // Supports staggered Spin-up?
#define AHCI_CAP_FBSS (1 << 16) // FIS-based switching supported?
#define AHCI_CAP_SSC (1 << 14) // Slumber state capable?
#define AHCI_CAP_PSC (1 << 13) // Partial state capable
#define AHCI_CAP_SALP (1 << 26) // Supports aggressive link power management

#define AHCI_CAP2_NVMHCI (1 << 1) // NVMHCI Present
#define AHCI_CAP2_BOHC (1 << 0) // BIOS/OS Handoff

#define AHCI_BOHC_BIOS_BUSY (1 << 4) // BIOS Busy
#define AHCI_BOHC_OS_OWNERSHIP (1 << 3) // OS Ownership Change

#define	SATA_SIG_SATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	0x96690101	// Port multiplier

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_SUD	0x0002
#define HBA_PxCMD_POD	0x0004
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000
#define HBA_PxCMD_ASP	0x4000000 // Aggressive Slumber/Partial
#define HBA_PxCMD_ICC 	(0xf << 28)
#define HBA_PxCMD_ICC_ACTIVE (1 << 28)

#define HBA_PORT_IPM_ACTIVE 1

#define HBA_PxSSTS_DET 0xfULL
#define HBA_PxSSTS_DET_INIT 1
#define HBA_PxSSTS_DET_PRESENT 3

#include <devicemanager.h>

namespace AHCI{
	enum AHCIStatus{
		Uninitialized = 0,
		Error = 1,
		Active = 2,
	};

	class Port : public DiskDevice{
	public:
		Port(int num, hba_port_t* portStructure, hba_mem_t* hbaMem);

		int ReadDiskBlock(uint64_t lba, uint32_t count, void* buffer);
		int WriteDiskBlock(uint64_t lba, uint32_t count, void* buffer);

        int blocksize = 512;
		AHCIStatus status = AHCIStatus::Uninitialized;
	private:
		int FindCmdSlot();
		int Access(uint64_t lba, uint32_t count, int write);

		hba_port_t* registers;

		hba_cmd_header_t* commandList; // Address Mapping of the Command List
		void* fis; // Address Mapping of the FIS

		hba_cmd_tbl_t* commandTables[8];

		uint64_t bufPhys;
		void* bufVirt;
	};

	int Init();

	inline void startCMD(hba_port_t *port)
	{
    	port->cmd &= ~HBA_PxCMD_ST;

		// Wait until CR (bit15) is cleared
		while (port->cmd & HBA_PxCMD_CR);

		// Set FRE (bit4) and ST (bit0)
		port->cmd |= HBA_PxCMD_FRE;
		port->cmd |= HBA_PxCMD_ST; 
	}
	
	// Stop command engine
	inline void stopCMD(hba_port_t *port)
	{
		// Clear ST (bit0)
		port->cmd &= ~HBA_PxCMD_ST;
	
		// Wait until FR (bit14), CR (bit15) are cleared
		while(1)
		{
			if (port->cmd & HBA_PxCMD_CR)
				continue;
			break;
		}
	
		// Clear FRE (bit4)
		port->cmd &= ~HBA_PxCMD_FRE;
	}
}