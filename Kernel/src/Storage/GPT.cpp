#include <Storage/GPT.h>

#include <Device.h>
#include <Logging.h>
#include <Memory.h>

#include <Debug.h>

namespace GPT {
int Parse(DiskDevice* disk) {
    gpt_header_t* header = (gpt_header_t*)kmalloc(disk->blocksize);

    if (int e = disk->ReadDiskBlock(1, disk->blocksize, (uint8_t*)header); e) {
        Log::Info("[GPT] Disk error %d", e);
        return -1; // Disk Error
    }

    if (header->signature != GPT_HEADER_SIGNATURE /*Check Signature*/ ||
        header->partNum == 0 /*Make sure there is at least 1 partition*/) {
        return 0; // GPT not present or corrupted
    }

    if (debugLevelPartitions >= DebugLevelNormal) {
        Log::Info("Found GPT Header Partitions: ");
        Log::Write(header->partNum);
        Log::Write(" Entry Size:");
        Log::Write(header->partEntrySize);
    }

    uint64_t tableLBA = header->partitionTableLBA;

    int partNum = 4; // header->partNum;

    gpt_entry_t* partitionTable = (gpt_entry_t*)kmalloc(partNum * header->partEntrySize);
    if (disk->ReadDiskBlock(tableLBA, partNum * header->partEntrySize, (uint8_t*)partitionTable)) {
        return -1; // Disk Error
    }

    for (int i = 0; i < partNum; i++) {
        gpt_entry_t entry = partitionTable[i];

        if (debugLevelPartitions >= DebugLevelNormal) {
            Log::Info("Found GPT Partition of size %d MB", (entry.endLBA - entry.startLBA) * 512 / 1024 / 1024);
        }

        if ((entry.endLBA - entry.startLBA)) {
            PartitionDevice* part = new PartitionDevice(entry.startLBA, entry.endLBA, disk);
            disk->partitions.add_back(part);
        }
    }

    kfree(partitionTable);
    kfree(header);
    return partNum;
}
} // namespace GPT