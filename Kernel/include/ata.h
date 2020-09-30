#pragma once

#include <stdint.h>

#define ATA_REGISTER_DATA 0
#define ATA_REGISTER_ERROR 1
#define ATA_REGISTER_FEATURES 1
#define ATA_REGISTER_SECTOR_COUNT 2
#define ATA_REGISTER_LBA_LOW 3
#define ATA_REGISTER_LBA_MID 4
#define ATA_REGISTER_LBA_HIGH 5
#define ATA_REGISTER_DRIVE_HEAD 6
#define ATA_REGISTER_STATUS 7
#define ATA_REGISTER_COMMAND 7

#define ATA_BMR_CMD 0
#define ATA_BMR_DEV_SPECIFIC_1 1
#define ATA_BMR_STATUS 2
#define ATA_BMR_DEV_SPECIFIC_2 3
#define ATA_BMR_PRDT_ADDRESS 4
#define ATA_BMR_CMD_SECONDARY 8
#define ATA_BMR_DEV_SPECIFIC_1_SECONDARY 9
#define ATA_BMR_STATUS_SECONDARY 10
#define ATA_BMR_DEV_SPECIFIC_2_SECONDARY 11
#define ATA_BMR_PRDT_ADDRESS_SECONDARY 12

#define ATA_DEV_BUSY     0x80    // Busy
#define ATA_DEV_DRDY    0x40    // Drive ready
#define ATA_DEV_DF      0x20    // Drive write fault
#define ATA_DEV_DSC     0x10    // Drive seek complete
#define ATA_DEV_DRQ     0x08    // Data request ready
#define ATA_DEV_CORR    0x04    // Corrected data
#define ATA_DEV_IDX     0x02    // Index
#define ATA_DEV_ERR     0x01    // Error

#define ATA_CMD_READ_DMA_EX     0x25
#define ATA_CMD_WRITE_DMA_EX    0x35
#define ATA_CMD_IDENTIFY        0xec

#define ATA_PRD_BUFFER(x) (x & 0xFFFFFFFF)
#define ATA_PRD_TRANSFER_SIZE(x) ((x & 0xFFFF) << 32)
#define ATA_PRD_END 0x8000000000000000ULL //(0x8000 << 48)

namespace ATA{
    class ATADiskDevice;

    void SendCommand(uint8_t drive, uint8_t command);

    int Init();
    int Access(ATADiskDevice* drive, uint64_t lba, uint16_t count, bool write = false);
}