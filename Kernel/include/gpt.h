#pragma once

#include <stdint.h>
#include <device.h>

#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL

typedef struct{
    uint64_t signature;
    uint32_t revision;
    uint32_t size;
    uint32_t crc32;
    uint32_t reserved;
    uint64_t currentLBA;
    uint64_t backupLBA;
    uint64_t firstLBA;
    uint64_t lastLBA;
    uint8_t diskGUID[16];
    uint64_t partitionTableLBA;
    uint32_t partNum;
    uint32_t partEntrySize;
    uint32_t partEntriesCRC;
} __attribute__((packed)) gpt_header_t;

typedef struct{
    uint8_t typeGUID[16];
    uint8_t partitionGUID[16];
    uint64_t startLBA;
    uint64_t endLBA;
    uint64_t flags;
    uint8_t name[72];
} __attribute__((packed)) gpt_entry_t;

namespace GPT{
    int Parse(DiskDevice* disk);
}