#include <elf.h>

#include <logging.h>
#include <string.h>
#include <scheduler.h>
#include <paging.h>
#include <physicalallocator.h>

int VerifyELF(void* elf){
    elf64_header_t elfHdr = *(elf64_header_t*)elf;

    char id[4];
    strncpy(id, (char*)elfHdr.id + 1, 3);
    if(strncmp("ELF", id, 3)){
        Log::Warning("Invalid ELF Header: ");
        Log::Write(id);
        return 0;
    } else return 1;
}

elf_info_t LoadELFSegments(process_t* proc, void* elf, uintptr_t base){
    elf_info_t elfInfo;
    memset(&elfInfo, 0, sizeof(elfInfo));

    if(!VerifyELF(elf)) return elfInfo; // Invalid ELF Header

    elf64_header_t elfHdr = *(elf64_header_t*)elf;

    elfInfo.entry = base + elfHdr.entry;
    elfInfo.phEntrySize = elfHdr.phEntrySize;
    elfInfo.phNum = elfHdr.phNum;

    for(int i = 0; i < elfHdr.phNum; i++){
        elf64_program_header_t elfPHdr = *((elf64_program_header_t*)(elf + elfHdr.phOff + i * elfHdr.phEntrySize));

        if(elfPHdr.memSize == 0) continue;
        for(int j = 0; j < (((elfPHdr.memSize + (elfPHdr.vaddr & (PAGE_SIZE_4K-1))) / PAGE_SIZE_4K)) + 1; j++){
            uint64_t phys = Memory::AllocatePhysicalMemoryBlock();
            Memory::MapVirtualMemory4K(phys,base + elfPHdr.vaddr + j * PAGE_SIZE_4K, 1, proc->addressSpace);
        }
    }

    char* linkPath = nullptr;

    for(int i = 0; i < elfHdr.phNum; i++){
        elf64_program_header_t elfPHdr = *((elf64_program_header_t*)(elf + elfHdr.phOff + i * elfHdr.phEntrySize));
        
        if(elfPHdr.type == PT_LOAD && elfPHdr.memSize > 0){
            memset((void*)base + elfPHdr.vaddr,0,elfPHdr.memSize);
            memcpy((void*)base + elfPHdr.vaddr,(void*)(elf + elfPHdr.offset),elfPHdr.fileSize);
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