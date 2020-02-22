#include <initrd.h>

#include <filesystem.h>
#include <string.h>
#include <memory.h>
#include <logging.h>
#include <panic.h>

namespace Initrd{
	fs_dirent_t* ReadDir(fs_node_t* node, uint32_t index);
	fs_node_t* FindDir(fs_node_t* node, char* name);

	uint32_t Read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer);
	uint32_t ReadNull(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer);

	void Open(fs_node_t* node, uint32_t flags);
	void Close(fs_node_t* node);
	
	uint32_t Write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer);

	lemoninitfs_header_t initrdHeader;
	lemoninitfs_node_t* nodes;
	fs_node_t* fsNodes;

	void* initrd_address;

	fs_node_t root;
	fs_dirent_t rootDirent;

	fs_node_t dev;
	fs_dirent_t devDirent;

	fs_node_t null;

	fs_dirent_t dirent;

	fs_node_t* devices[64];
	uint32_t deviceCount = 0;

	void Initialize(uintptr_t address, uint32_t size) {
		#ifdef Lemon32
		void* virtual_address = (void*)Memory::KernelAllocateVirtualPages(size / PAGE_SIZE + 1);
		Memory::MapVirtualPages(address, (uint32_t)virtual_address, size / PAGE_SIZE + 1);
		initrd_address = virtual_address;
		#endif
		#ifdef Lemon64
		initrd_address = (void*)address;
		#endif
		Log::Info("Initrd address: ");
		Log::Info(address);
		Log::Info("Size:");
		Log::Info(size, false);
		Log::Info("File Count:");
		Log::Info(*((uint32_t*)initrd_address), false);
		Log::Write("files\r\n");
		initrdHeader = *(lemoninitfs_header_t*)initrd_address;
		nodes = (lemoninitfs_node_t*)(initrd_address + sizeof(lemoninitfs_header_t));
		
		strcpy(root.name,"");
		root.flags = FS_NODE_DIRECTORY;
		root.inode = 0;
		strcpy(rootDirent.name, "dev");

		root.readDir = Initrd::ReadDir;
		root.findDir = Initrd::FindDir;

		strcpy(dev.name,"dev");
		dev.flags = FS_NODE_DIRECTORY;
		dev.inode = 0;
		strcpy(devDirent.name, "dev");
		devDirent.type = FS_NODE_DIRECTORY;

		dev.readDir = Initrd::ReadDir;
		dev.findDir = Initrd::FindDir;

		strcpy(null.name,"null");
		null.flags = FS_NODE_FILE;
		null.inode = 0;

		null.read = ReadNull;
		fsNodes = (fs_node_t*)kmalloc(sizeof(fs_node_t)*initrdHeader.fileCount);

		for(int i = 0; i < initrdHeader.fileCount; i++){
			fs_node_t* node = &(fsNodes[i]);
			strcpy(node->name,nodes[i].filename);
			node->inode = i;
			node->flags = FS_NODE_FILE;
			node->read = Initrd::Read;
			node->write = Initrd::Write;
			node->open = Initrd::Open;
			node->close = Initrd::Close;
			node->size = nodes[i].size;
		}
	}

	uint32_t Read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
		lemoninitfs_node_t inode = nodes[node->inode];

		if(offset > inode.size) return 0;
		else if(offset + size > inode.size || size > inode.size) size = inode.size - offset;

		if(!size) return 0;

		memcpy(buffer, (void*)((uintptr_t)inode.offset + (uintptr_t)initrd_address + offset), size);
		return size;
	}

	uint32_t ReadNull(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
		// /if(offset > inode.length || offset + size > inode.length) return 0;

		memset(buffer, -1, size);
		return size;
	}

	uint32_t Write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
		return 0; // It's a ramdisk.
	}

	void Open(fs_node_t* node, uint32_t flags){
		
	}

	void Close(fs_node_t* node){
		
	}

	fs_dirent_t* ReadDir(fs_node_t* node, uint32_t index){
			Log::Info(index);
		if(node == &root && index == 0){
			return &devDirent;
		} else if(node == &dev){
			if(index >= deviceCount) return NULL;
			memset(dirent.name,0,128); // Zero the string
			strcpy(dirent.name,devices[index]->name);
			dirent.inode = index;
			dirent.type = 0;
			Log::Info(dirent.name);
			return &dirent;
		} else if (index >= initrdHeader.fileCount + 1){ // All files in initrd + /dev
			return NULL;
		}
		memset(dirent.name,0,128); // Zero the string
		strcpy(dirent.name,nodes[index-1].filename);
		dirent.inode = index-1;
		dirent.type = 0;

		return &dirent;
	}

	fs_node_t* FindDir(fs_node_t* node, char* name){
		if(node == &root){
			if(strcmp(name,dev.name) == 0) return &dev;
			
			Log::Info(initrdHeader.fileCount, false);
			for(int i = 0; i < initrdHeader.fileCount;i++){
				if(strcmp(fsNodes[i].name,name) == 0) return &(fsNodes[i]);
			}
		} else if(node == &dev){
			for(int i = 0; i < deviceCount; i++)
				if(strcmp(devices[i]->name, name) == 0) return devices[0];
		} else {
			const char* pr[] = {"could not find dir"};
			KernelPanic(pr,1);
		}
		return NULL;
	}


	lemoninitfs_node_t* List() {
		return nodes;
	}

	lemoninitfs_header_t* GetHeader() {
		return &initrdHeader;
	}

	void* Read(int node) {
		return (void*)(nodes[node].offset+(uintptr_t)initrd_address);
	}

	uint8_t* Read(lemoninitfs_node_t node, uint8_t* buffer) {
		memcpy((void*)buffer, (void*)(initrd_address + node.offset), node.size);
		return buffer;
	}

	void* Read(char* filename) {
		for (uint32_t i = 0; i < initrdHeader.fileCount; i++) {
			if (nodes[i].filename == filename)
				return (void*)(nodes[i].offset+(uintptr_t)initrd_address);
		}
		return 0;
	}

	/*lemoninitfs_node_t GetNode(char* filename) {
		for (uint32_t i = 0; i < initrdHeader.fileCount; i++) {
			if (nodes[i].filename == filename)
				return nodes[i];
		}
		return nodes[0];
	}*/

	fs_node_t* GetRoot(){
		return &root;
	}

	void RegisterDevice(fs_node_t* device){
		devices[deviceCount++] = device;
	}
}