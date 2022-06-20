#include <Storage/NVMe.h>

#include <Debug.h>
#include <Errno.h>
#include <Storage/GPT.h>

namespace NVMe {
Namespace::Namespace(Controller* controller, uint32_t nsID, const NamespaceIdentity& id) {
    this->controller = controller; // Controller
    this->nsID = nsID;             // Namespace ID

    diskSize = id.namespaceSize;

    uint8_t lbaFormat = id.fmtLBASize & 0xf; // Low 4 bits are LBA format
    uint8_t lbaSize = id.lbaFormats[lbaFormat].lbaDataSize;
    if (lbaSize < 9) {
        nsStatus = NamespaceStatus::Error;
        return; // LBA Size must be at least 9 (512 bytes)
    }

    blocksize = 1 << lbaSize;

    for (unsigned i = 0; i < 8; i++) {
        physBuffers[i] = Memory::AllocatePhysicalMemoryBlock();
        buffers[i] = Memory::KernelAllocate4KPages(1);
        Memory::KernelMapVirtualMemory4K(physBuffers[i], (uintptr_t)buffers[i], 1);

        bufferLocks[i] = 0;
    }

    nsStatus = NamespaceStatus::Active;

    switch (GPT::Parse(this)) {
    case 0:
        Log::Error("[NVMe] Disk has a corrupted or non-existant GPT. MBR disks are NOT supported.");
        break;
    case -1:
        Log::Error("[NVMe] Disk Error while Parsing GPT for namespace");
        break;
    }
    Log::Info("[NVMe] (NSID: %d) Blocksize is %d and found %d partitions!", nsID, blocksize, partitions.get_length());

    InitializePartitions();
}

int Namespace::AcquireBuffer() {
    if (bufferAvailability.Wait()) {
        return -EINTR;
    }

    for (uint8_t i = 0; i < 8; i++) {
        if (!acquireTestLock(&bufferLocks[i])) {
            return i;
        }
    }

    return -1;
}

void Namespace::ReleaseBuffer(int buffer) {
    assert(buffer >= 0 && buffer < 8);
    releaseLock(&bufferLocks[buffer]);

    bufferAvailability.Signal();
}

int Namespace::ReadDiskBlock(uint64_t lba, uint32_t count, void* _buffer) {
    if (lba + count > diskSize) {
        return 2;
    }

    uint32_t blockCount = (count + (blocksize - 1)) / blocksize;
    uint8_t* buffer = reinterpret_cast<uint8_t*>(_buffer);
    int blockBufferIndex = AcquireBuffer();
    if (blockBufferIndex == -EINTR) {
        return -EINTR;
    }
    assert(blockBufferIndex >= 0);

    NVMeQueue* queue = controller->AcquireIOQueue();
    if (!queue) {
        return -EINTR; // The only time it should be null is if we got interrupted
    }

    NVMeCompletion completion;

    NVMeCommand cmd;
    memset(&cmd, 0, sizeof(NVMeCommand));
    cmd.opcode = NVMCommands::NVMCmdRead;

    cmd.read.blockNum = 1;
    cmd.read.startLBA = lba;
    cmd.prp1 = physBuffers[blockBufferIndex];
    cmd.nsID = nsID;

    while (blockCount && count > 0) {
        int size = blocksize;
        if (size > count)
            size = count;

        if (!size)
            break;

        queue->SubmitWait(cmd, completion);

        if (completion.status > 0) {
            controller->ReleaseIOQueue(queue);
            ReleaseBuffer(blockBufferIndex);

            IF_DEBUG(debugLevelNVMe >= DebugLevelNormal,
                     { Log::Error("[NVMe] (NSID: %d, LBA: %x) Disk Error %d", nsID, lba, completion.status); });
            return -completion.status;
        }

        memcpy(buffer, buffers[blockBufferIndex], size);

        count -= size;
        buffer += size;
        cmd.read.startLBA++;
        blockCount--;
    }

    ReleaseBuffer(blockBufferIndex);
    controller->ReleaseIOQueue(queue);
    return 0;
}

int Namespace::WriteDiskBlock(uint64_t lba, uint32_t count, void* _buffer) {
    if (lba + count > diskSize) {
        return 2;
    }

    uint32_t blockCount = (count + (blocksize - 1)) / blocksize;
    uint8_t* buffer = reinterpret_cast<uint8_t*>(_buffer);
    int blockBufferIndex = AcquireBuffer();
    if (blockBufferIndex == -EINTR) {
        return -EINTR;
    }

    assert(blockBufferIndex >= 0);

    NVMeQueue* queue = controller->AcquireIOQueue();
    if (!queue) {
        return -EINTR; // The only time it should be null is if we got interrupted
    }

    NVMeCompletion completion;

    NVMeCommand cmd;
    memset(&cmd, 0, sizeof(NVMeCommand));
    cmd.opcode = NVMCommands::NVMCmdWrite;

    cmd.write.blockNum = 1;
    cmd.write.startLBA = lba;
    cmd.prp1 = physBuffers[blockBufferIndex];
    cmd.nsID = nsID;

    while (blockCount && count > 0) {
        int size = blocksize;
        if (size > count)
            size = count;

        if (!size)
            break;

        memcpy(buffers[blockBufferIndex], buffer, size);

        queue->SubmitWait(cmd, completion);

        if (completion.status > 0) {
            controller->ReleaseIOQueue(queue);
            ReleaseBuffer(blockBufferIndex);

            IF_DEBUG(debugLevelNVMe >= DebugLevelNormal,
                     { Log::Error("[NVMe] (NSID: %d, LBA: %x) Disk Error %d", nsID, lba, completion.status); });
            return -completion.status;
        }

        count -= size;
        buffer += size;
        cmd.read.startLBA++;
        blockCount--;
    }

    ReleaseBuffer(blockBufferIndex);
    controller->ReleaseIOQueue(queue);
    return 0;
}
} // namespace NVMe