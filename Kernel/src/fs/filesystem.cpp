#include <fs/filesystem.h>

#include <fs/fsvolume.h>
#include <logging.h>


namespace fs{
	
	class Root : public FsNode {
	public:
		Root() { flags = FS_NODE_DIRECTORY; }

		int ReadDir(DirectoryEntry*, uint32_t);
		FsNode* FindDir(char* name);
	};
	
	class Dev : public FsNode {
	public:
		Dev() { flags = FS_NODE_DIRECTORY; }

		int ReadDir(DirectoryEntry*, uint32_t);
		FsNode* FindDir(char* name);
	};
	
	class Null : public FsNode {
	public:
		Null() { flags = FS_NODE_FILE; }

		ssize_t Read(size_t, size_t, uint8_t *);
		ssize_t Write(size_t, size_t, uint8_t *);
	};

    Root root;
	DirectoryEntry rootDirent = DirectoryEntry(&root, "");

	Dev dev;
	DirectoryEntry devDirent = DirectoryEntry(&dev, "dev");

	Null null;
	DirectoryEntry nullDirent = DirectoryEntry(&dev, "null");

	List<FsVolume*>* volumes;
    
	DirectoryEntry* devices[64];
	uint32_t deviceCount = 0;

    void Initialize(){
		volumes = new List<FsVolume*>();

		root.flags = FS_NODE_DIRECTORY;
		root.inode = 0;
		rootDirent.flags = FS_NODE_DIRECTORY;
		strcpy(rootDirent.name, "");

		dev.flags = FS_NODE_DIRECTORY;
		dev.inode = 0;
		strcpy(devDirent.name, "dev");
		devDirent.flags = FS_NODE_DIRECTORY;

		strcpy(nullDirent.name,"null");
		null.flags = FS_NODE_CHARDEVICE;
		null.inode = 0;
		null.size = 1;

        RegisterDevice(&nullDirent);
    }

	void RegisterVolume(FsVolume* vol){
		vol->mountPoint->parent = &root;
		volumes->add_back(vol);
	}

    FsNode* GetRoot(){
        return &root;
    }

	FsNode* ResolvePath(char* path, char* workingDir){
		Log::Info("Resolving: %s", path);

		char* tempPath;
		if(workingDir && path[0] != '/'){
			tempPath = (char*)kmalloc(strlen(path) + strlen(workingDir) + 2);
			strcpy(tempPath, workingDir);
			strcpy(tempPath + strlen(tempPath), "/");
			strcpy(tempPath + strlen(tempPath), path);
		} else {
			tempPath = (char*)kmalloc(strlen(path) + 1);
			strcpy(tempPath, path);
		}

		FsNode* root = fs::GetRoot();
		FsNode* current_node = root;

		char* file = strtok(tempPath,"/");

		while(file != NULL){ // Iterate through the directories to find the file
			FsNode* node = fs::FindDir(current_node,file);
			if(!node) {
				Log::Warning("%s not found!", path);
				return nullptr;
			}
			if((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
				current_node = node;
				file = strtok(NULL, "/");
				continue;
			}

			if(file = strtok(NULL, "/")){
				Log::Warning("Found file in the path however we were not finished");
				return nullptr;
			}

			current_node = node;
			break;
		}

		kfree(tempPath);
		return current_node;
	}
	
	FsNode* ResolveParent(char* path, char* workingDir){
		char* pathCopy = (char*)kmalloc(strlen(path) + 1);
		strcpy(pathCopy, path);

		if(pathCopy[strlen(pathCopy) - 1] == '/'){ // Remove trailing slash
			pathCopy[strlen(pathCopy) - 1] = 0;
		}

		char* dirPath = strrchr(pathCopy, '/');

		FsNode* parentDirectory = nullptr;

		if(dirPath == nullptr){
			parentDirectory = fs::ResolvePath(workingDir);
		} else {
			*(dirPath - 1) = 0; // Cut off the directory name from the path copy
			parentDirectory = fs::ResolvePath(pathCopy, workingDir);
		}

		kfree(pathCopy);
		return parentDirectory;
	}

	char* CanonicalizePath(const char* path, char* workingDir){
		char* tempPath;
		if(workingDir && path[0] != '/'){
			tempPath = (char*)kmalloc(strlen(path) + strlen(workingDir) + 2);
			strcpy(tempPath, workingDir);
			strcpy(tempPath + strlen(tempPath), "/");
			strcpy(tempPath + strlen(tempPath), path);
		} else {
			tempPath = (char*)kmalloc(strlen(path) + 1);
			strcpy(tempPath, path);
		}

		char* file = strtok(tempPath,"/");
		List<char*>* tokens = new List<char*>();

		while(file != NULL){
			tokens->add_back(file);
			file = strtok(NULL, "/");
		}

		int newLength = 2; // Separator and null terminator
		newLength += strlen(path) + strlen(workingDir);
		for(int i = 0; i < tokens->get_length(); i++){
			if(strlen(tokens->get_at(i)) == 0){
				tokens->remove_at(i--);
				continue;
			} else if(strcmp(tokens->get_at(i), ".") == 0){
				tokens->remove_at(i--);
				continue;
			} else if(strcmp(tokens->get_at(i), "..") == 0){
				if(i){
					tokens->remove_at(i);
					tokens->remove_at(i - 1);
					i -= 2;
				} else tokens->remove_at(i);
				continue;
			}

			newLength += strlen(tokens->get_at(i)) + 1; // Name and separator
		}

		char* outPath = (char*)kmalloc(newLength);
		outPath[0] = 0;

		if(!tokens->get_length()) strcpy(outPath + strlen(outPath), "/");
		else for(int i = 0; i < tokens->get_length(); i++){
			strcpy(outPath + strlen(outPath), "/");
			strcpy(outPath + strlen(outPath), tokens->get_at(i));
		}

		kfree(tempPath);
		delete tokens;

		return outPath;
	}

	char* BaseName(const char* path){
		char* pathCopy = (char*)kmalloc(strlen(path));
		strcpy(pathCopy, path);
		
		if(pathCopy[strlen(pathCopy) - 1] == '/'){ // Remove trailing slash
			pathCopy[strlen(pathCopy) - 1] == 0;
		}

		char* basename = nullptr;
		char* temp;
		if((temp = strrchr(pathCopy, '/'))){
			basename = (char*)kmalloc(strlen(temp));
			strcpy(basename, temp);

			kfree(pathCopy);
		} else {
			basename = pathCopy;
		}

		return basename;
	}

	void RegisterDevice(DirectoryEntry* device){
		Log::Info("Device Registered: ");
		Log::Write(device->name);
		devices[deviceCount++] = device;
	}
    
	int Dev::ReadDir(DirectoryEntry* dirent, uint32_t index){
		if(index >= deviceCount) return -1;
		strcpy(dirent->name,devices[index]->name);

		return 0;
	}

    FsNode* Dev::FindDir(char* name){
		for(unsigned i = 0; i < deviceCount; i++)
			if(strcmp(devices[i]->name, name) == 0) return devices[i]->node;

        return NULL;
	}

	int Root::ReadDir(DirectoryEntry* dirent, uint32_t index){
		if(index == 0){
			*dirent = devDirent;
			return 0;
		} else if (index < fs::volumes->get_length() + 1){
			*dirent = (volumes->get_at(index - 1)->mountPointDirent);
			return 0;
		} else return -1;
	}

    FsNode* Root::FindDir(char* name){
		if(strcmp(name,devDirent.name) == 0) return &dev;

		for(int i = 0; i < fs::volumes->get_length(); i++){
			if(strcmp(fs::volumes->get_at(i)->mountPointDirent.name,name) == 0) return (fs::volumes->get_at(i)->mountPointDirent.node);
		}

        return NULL;
	}

	ssize_t Null::Read(size_t offset, size_t size, uint8_t *buffer){

		memset(buffer, -1, size);
		return size;
	}
	
	ssize_t Null::Write(size_t offset, size_t size, uint8_t *buffer){
		return size;
	}

    ssize_t Read(FsNode* node, size_t offset, size_t size, uint8_t *buffer){
		assert(node);

		if((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK) return Read(node->link, offset, size, buffer);

        return node->Read(offset,size,buffer);
    }

    ssize_t Write(FsNode* node, size_t offset, size_t size, uint8_t *buffer){
		assert(node);

		if((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK) return Write(node->link, offset, size, buffer);

        return node->Write(offset,size,buffer);
    }

    fs_fd_t* Open(FsNode* node, uint32_t flags){
        return node->Open(flags);
    }

    void Close(FsNode* node){
        return node->Close();
    }

    void Close(fs_fd_t* fd){
		if(!fd) return;

        fd->node->Close();

		kfree(fd);
    }

    int ReadDir(FsNode* node, DirectoryEntry* dirent, uint32_t index){
		assert(node);

		if((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK) return ReadDir(node->link, dirent, index);

        return node->ReadDir(dirent, index);
    }

    FsNode* FindDir(FsNode* node, char* name){
		assert(node);

		if((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK) return FindDir(node->link, name);
            
		return node->FindDir(name);
    }
	
    ssize_t Read(fs_fd_t* handle, size_t size, uint8_t *buffer){
        if(handle->node){
            ssize_t ret = Read(handle->node,handle->pos,size,buffer);

			if(ret >= 0){
				handle->pos += ret;
			}
			
			return ret;
		}
        else return 0;
    }

    ssize_t Write(fs_fd_t* handle, size_t size, uint8_t *buffer){
        if(handle->node){
            off_t ret = Write(handle->node,handle->pos,size,buffer);

			if(ret >= 0){
				handle->pos += ret;
			}
			
			return ret;
		} else return 0;
    }

    int ReadDir(fs_fd_t* handle, DirectoryEntry* dirent, uint32_t index){
        if(handle->node)
            return ReadDir(handle->node, dirent, index);
        else return 0;
    }

    FsNode* FindDir(fs_fd_t* handle, char* name){
        if(handle->node)
            return FindDir(handle->node,name);
        else return 0;
    }

	int Ioctl(fs_fd_t* handle, uint64_t cmd, uint64_t arg){
		if(handle->node) return handle->node->Ioctl(cmd, arg);
		else return -1;
	}
}