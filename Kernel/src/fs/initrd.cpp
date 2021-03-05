/*
 *
 * Old Initrd Format, no longer used
 * 
 */

#include <Fs/Initrd.h>

#include <Filesystem.h>
#include <String.h>
#include <Memory.h>
#include <Logging.h>
#include <Panic.h>
#include <List.h>
#include <Fs/FsVolume.h>

namespace Initrd{
	int ReadDir(fs_node_t* node, fs_dirent_t* dirent, uint32_t index);
	fs_node_t* FindDir(fs_node_t* node, char* name);

	size_t Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer);
	size_t ReadNull(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer);

	void Open(fs_node_t* node, uint32_t flags);
	void Close(fs_node_t* node);
	
	size_t Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer);

	lemoninitfs_header_t initrdHeader;
	lemoninitfs_node_t* nodes;
	fs_node_t* fsNodes;

	//fs_dirent_t dirent;

	void* initrd_address;

	InitrdVolume* vol;

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

		vol = new InitrdVolume();

		fs::volumes->add_back(vol);
		
		InitrdVolume* vol2 = new InitrdVolume(); // Very Cheap Workaround
		strcpy(vol2->mountPoint.name, "lib");
		strcpy(vol2->mountPointDirent.name, "lib");

		fs::volumes->add_back(vol2);
	}

	size_t Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
		lemoninitfs_node_t inode = nodes[node->inode];

		if(offset > inode.size) return 0;
		else if(offset + size > inode.size || size > inode.size) size = inode.size - offset;

		if(!size) return 0;

		memcpy(buffer, (void*)((uintptr_t)inode.offset + (uintptr_t)initrd_address + offset), size);
		return size;
	}

	size_t Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
		return 0; // It's a ramdisk.
	}

	void Open(fs_node_t* node, uint32_t flags){
		
	}

	void Close(fs_node_t* node){
		
	}

	int ReadDir(fs_node_t* node, fs_dirent_t* dirent, uint32_t index){
		if(index >= initrdHeader.fileCount + 2) return -1;

		if(index == 0) {
			memset(dirent->name,0,128); // Zero the string
			strcpy(dirent->name,".");
			dirent->inode = 0;
			dirent->type = FS_NODE_DIRECTORY;
			return 0;
		} else if (index == 1){
			memset(dirent->name,0,128); // Zero the string
			strcpy(dirent->name,"..");
			dirent->inode = 2;
			dirent->type = FS_NODE_DIRECTORY;
			return 0;
		}
		memset(dirent->name,0,128); // Zero the string
		strcpy(dirent->name,nodes[index - 2].filename);
		dirent->inode = index - 3;
		dirent->type = 0;

		return 0;
	}

	fs_node_t* FindDir(fs_node_t* node, char* name) {
		for(int i = 0; i < initrdHeader.fileCount;i++){
			if(strcmp(fsNodes[i].name,name) == 0) return &(fsNodes[i]);
		}

		if(strcmp(".", name) == 0) return node;
		if(strcmp("..", name) == 0) return fs::GetRoot();

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

	InitrdVolume::InitrdVolume(){
		strcpy(mountPoint.name, "initrd");
		mountPoint.flags = FS_NODE_DIRECTORY | FS_NODE_MOUNTPOINT;

		mountPoint.read = Initrd::Read;
		mountPoint.write = Initrd::Write;
		mountPoint.open = Initrd::Open;
		mountPoint.close = Initrd::Close;
		mountPoint.readDir = Initrd::ReadDir;
		mountPoint.findDir = Initrd::FindDir;

		strcpy(mountPointDirent.name, "initrd");
		mountPointDirent.type = FS_NODE_DIRECTORY;
	}

	size_t InitrdVolume::Read(struct fs_node* node, size_t offset, size_t size, uint8_t *buffer){
		return Initrd::Read(node, offset, size, buffer);
	}
	
	size_t InitrdVolume::Write(struct fs_node* node, size_t offset, size_t size, uint8_t *buffer){
		return Initrd::Write(node, offset, size, buffer);
	}
	
	void InitrdVolume::Open(struct fs_node* node, uint32_t flags){
		Initrd::Open(node, flags);
	}

	void InitrdVolume::Close(struct fs_node* node){
		Initrd::Close(node);
	}

	int InitrdVolume::ReadDir(struct fs_node* node, fs_dirent_t* dirent, uint32_t index){
		return Initrd::ReadDir(node, dirent, index);
	}

	fs_node* InitrdVolume::FindDir(struct fs_node* node, char* name){
		return Initrd::FindDir(node, name);
	}
}