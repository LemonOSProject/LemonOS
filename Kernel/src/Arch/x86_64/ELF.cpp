#include <ELF.h>

#include <Logging.h>
#include <String.h>
#include <Scheduler.h>
#include <Paging.h>
#include <PhysicalAllocator.h>
#include <Scheduler.h>
#include <Math.h>

int VerifyELF(void* elf){
    elf64_header_t elfHdr = *(elf64_header_t*)elf;

    char id[4];
    strncpy(id, (char*)elfHdr.id + 1, 3);
    if(strncmp("ELF", id, 3)){
        Log::Warning("Invalid ELF Header: %x, %x, %x (%c%c%c)", id[0], id[1], id[2], id[0], id[1], id[2]);
        return 0;
    } else return 1;
}

elf_info_t LoadELFSegments(process_t* proc, void* _elf, uintptr_t base){
    uint8_t* elf = reinterpret_cast<uint8_t*>(_elf);
    CPU* cpuLocal = GetCPULocal();

    elf_info_t elfInfo;
    memset(&elfInfo, 0, sizeof(elfInfo));

    if(!VerifyELF(elf)) return elfInfo; // Invalid ELF Header

    elf64_header_t elfHdr = *(elf64_header_t*)elf;

    elfInfo.entry = base + elfHdr.entry;
    elfInfo.phEntrySize = elfHdr.phEntrySize;
    elfInfo.phNum = elfHdr.phNum;

    for(uint16_t i = 0; i < elfHdr.phNum; i++){
        elf64_program_header_t elfPHdr = *((elf64_program_header_t*)(elf + elfHdr.phOff + i * elfHdr.phEntrySize));

        if(elfPHdr.memSize == 0) continue;
        for(unsigned j = 0; j < ((elfPHdr.memSize + (elfPHdr.vaddr & 0xFFF) + 0xFFF) >> 12); j++){
            uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
            Memory::MapVirtualMemory4K(phys, base + (elfPHdr.vaddr & ~0xFFFUL) + j * PAGE_SIZE_4K, 1, proc->addressSpace);
            
            proc->usedMemoryBlocks++;
        }
    }

    char* linkPath = nullptr;
    uintptr_t pml4Phys = Scheduler::GetCurrentProcess()->addressSpace->pml4Phys;

    for(int i = 0; i < elfHdr.phNum; i++){
        elf64_program_header_t elfPHdr = *((elf64_program_header_t*)(elf + elfHdr.phOff + i * elfHdr.phEntrySize));
        
        if(elfPHdr.type == PT_LOAD && elfPHdr.memSize > 0){
            acquireLock(&cpuLocal->runQueueLock);
            asm("cli");
            asm volatile("mov %%rax, %%cr3" :: "a"(proc->addressSpace->pml4Phys));
            memset((void*)(base + elfPHdr.vaddr),0,elfPHdr.memSize);
            memcpy((void*)(base + elfPHdr.vaddr),(void*)(elf + elfPHdr.offset), elfPHdr.fileSize);
            asm volatile("mov %%rax, %%cr3" :: "a"(pml4Phys));
            asm("sti");
            releaseLock(&cpuLocal->runQueueLock);
        } else if (elfPHdr.type == PT_PHDR) {
            elfInfo.pHdrSegment = base + elfPHdr.vaddr;
        } else if(elfPHdr.type == PT_INTERP){
            linkPath = (char*)kmalloc(elfPHdr.fileSize + 1);
            strncpy(linkPath, (char*)(elf + elfPHdr.offset), elfPHdr.fileSize);
            linkPath[elfPHdr.fileSize] = 0; // Null terminate the path

            elfInfo.linkerPath = linkPath;
        }
    }

    return elfInfo;
}