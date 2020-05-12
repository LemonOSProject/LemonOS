#include <filesystem.h>

#include <fsvolume.h>
#include <logging.h>

namespace fs{
    size_t ReadNull(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer);
	size_t WriteNull(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer);

    int RootReadDir(fs_node_t* node, fs_dirent_t* dirent, uint32_t index);
    fs_node_t* RootFindDir(fs_node_t* node, char* name);
    int DevReadDir(fs_node_t* node, fs_dirent_t* dirent, uint32_t index);
    fs_node_t* DevFindDir(fs_node_t* node, char* name);

    fs_node_t root;
	fs_dirent_t rootDirent;

	fs_node_t dev;
	fs_dirent_t devDirent;

	fs_node_t null;
    
	fs_dirent_t dirent;

	List<FsVolume*>* volumes;
    
	fs_node_t* devices[64];
	uint32_t deviceCount = 0;

    void Initialize(){
		volumes = new List<FsVolume*>();

		strcpy(root.name,"");
		root.flags = FS_NODE_DIRECTORY;
		root.inode = 0;
		strcpy(rootDirent.name, "");

		root.readDir = RootReadDir;
		root.findDir = RootFindDir;

		strcpy(dev.name,"dev");
		dev.flags = FS_NODE_DIRECTORY;
		dev.inode = 0;
		strcpy(devDirent.name, "dev");
		devDirent.type = FS_NODE_DIRECTORY;

		dev.readDir = DevReadDir;
		dev.findDir = DevFindDir;

		strcpy(null.name,"null");
		null.flags = FS_NODE_CHARDEVICE;
		null.inode = 0;
		null.size = 1;

		null.read = ReadNull;
		null.write = WriteNull;

        RegisterDevice(&null);
    }

    fs_node_t* GetRoot(){
        return &root;
    }

	fs_node_t* ResolvePath(char* path, char* workingDir){
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

		fs_node_t* root = fs::GetRoot();
		fs_node_t* current_node = root;

		char* file = strtok(tempPath,"/");

		while(file != NULL){ // Iterate through the directories to find the file
			fs_node_t* node = fs::FindDir(current_node,file);
			if(!node) {
				Log::Warning(tempPath);
				Log::Write(" not found!");
				return nullptr;
			}
			if(node->flags & FS_NODE_DIRECTORY){
				current_node = node;
				file = strtok(NULL, "/");
				continue;
			}
			current_node = node;
			break;
		}

		kfree(tempPath);
		Log::Info("Returning: %s", current_node->name);
		return current_node;
	}

	char* CanonicalizePath(char* path, char* workingDir){
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

	void RegisterDevice(fs_node_t* device){
		Log::Info("Device Registered: ");
		Log::Write(device->name);
		devices[deviceCount++] = device;
	}
    
	int RootReadDir(fs_node_t* node, fs_dirent_t* dirent, uint32_t index){
		if(node == &root && index == 0){
			*dirent = devDirent;
			return 0;
		} else if (node == &root && index < fs::volumes->get_length() + 1){
			*dirent = (volumes->get_at(index - 1)->mountPointDirent);
			return 0;
		} else return -1;
	}

    fs_node_t* RootFindDir(fs_node_t* node, char* name){
		if(node == &root){
			if(strcmp(name,dev.name) == 0) return &dev;

			for(int i = 0; i < fs::volumes->get_length(); i++){
				if(strcmp(fs::volumes->get_at(i)->mountPoint.name,name) == 0) return &(fs::volumes->get_at(i)->mountPoint);
			}
			
		}

        return NULL;
	}
    
	int DevReadDir(fs_node_t* node, fs_dirent_t* dirent, uint32_t index){
		if(node == &dev){
			if(index >= deviceCount) return -1;
			memset(dirent->name,0,128); // Zero the string
			strcpy(dirent->name,devices[index]->name);
			dirent->inode = index;
			dirent->type = 0;
			return 0;
		} else return -2;
	}

    fs_node_t* DevFindDir(fs_node_t* node, char* name){
		if(node == &dev){
			for(unsigned i = 0; i < deviceCount; i++)
				if(strcmp(devices[i]->name, name) == 0) return devices[i];
		}

        return NULL;
	}

	size_t ReadNull(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){

		memset(buffer, -1, size);
		return size;
	}
	
	size_t WriteNull(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
		return size;
	}

    size_t Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
		if(!node) return 0;

		if(node->flags & FS_NODE_SYMLINK) return Read(node->link, offset, size, buffer);

        if(node->read)
            return node->read(node,offset,size,buffer);
		else return 0;
    }

    size_t Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
		if(!node) return 0;

		if(node->flags & FS_NODE_SYMLINK) return Write(node->link, offset, size, buffer);

        if(node->write)
            return node->write(node,offset,size,buffer);
		else return 0;
    }

    fs_fd_t* Open(fs_node_t* node, uint32_t flags){
        if(node->open){
            node->open(node,flags);
        }

		fs_fd_t* fd = new fs_fd_t;
		fd->pos = 0;
		fd->node = node;
		fd->mode = flags;

		return fd;
    }

    void Close(fs_node_t* node){
        if(node->close){
            return node->close(node);
        }
    }

    void Close(fs_fd_t* fd){
		if(!fd) return;

        if(fd->node->close){
            fd->node->close(fd->node);
        }

		delete fd;
    }

    int ReadDir(fs_node_t* node, fs_dirent_t* dirent, uint32_t index){
		if(!node) return 0;

		if(node->flags & FS_NODE_SYMLINK) return ReadDir(node->link, dirent, index);

        if(node->readDir)
            return node->readDir(node, dirent, index);
		else return 0;
    }

    fs_node_t* FindDir(fs_node_t* node, char* name){
		if(!node) return 0;

		if(node->flags & FS_NODE_SYMLINK) return FindDir(node->link, name);

        if(node->findDir)
            return node->findDir(node, name);
		else return 0;
    }
	
    size_t Read(fs_fd_t* handle, size_t size, uint8_t *buffer){
        if(handle->node){
            off_t ret = Read(handle->node,handle->pos,size,buffer);
			handle->pos += ret;
			return ret;
		}
        else return 0;
    }

    size_t Write(fs_fd_t* handle, size_t size, uint8_t *buffer){
        if(handle->node){
            off_t ret = Write(handle->node,handle->pos,size,buffer);
			handle->pos += ret;
			return ret;
		} else return 0;
    }

    int ReadDir(fs_fd_t* handle, fs_dirent_t* dirent, uint32_t index){
        if(handle->node)
            return ReadDir(handle->node, dirent, index);
        else return 0;
    }

    fs_node_t* FindDir(fs_fd_t* handle, char* name){
        if(handle->node)
            return FindDir(handle->node,name);
        else return 0;
    }

	int Ioctl(fs_fd_t* handle, uint64_t cmd, uint64_t arg){
		if(handle->node && handle->node->ioctl) return handle->node->ioctl(handle->node, cmd, arg);
		else return -1;
	}
}