#include <fs/filesystem.h>

#include <fs/fsvolume.h>
#include <logging.h>
#include <errno.h>

#include <debug.h>

namespace fs{
	volume_id_t nextVID = 1; // Next volume ID
	
	class Root : public FsNode {
	public:
		Root() {
			inode = 0;
			flags = FS_NODE_DIRECTORY;
		}

		int ReadDir(DirectoryEntry*, uint32_t);
		FsNode* FindDir(char* name);
	};

    Root root;
	DirectoryEntry rootDirent = DirectoryEntry(&root, "");

	List<FsVolume*>* volumes;
    
	DirectoryEntry* devices[64];
	uint32_t deviceCount = 0;

    void Initialize(){
		volumes = new List<FsVolume*>();
    }

	volume_id_t GetVolumeID(){
		return nextVID++;
	}

	void RegisterVolume(FsVolume* vol){
		vol->mountPoint->parent = &root;
		vol->SetVolumeID(GetVolumeID());
		volumes->add_back(vol);
	}

    FsNode* GetRoot(){
        return &root;
    }

	FsNode* FollowLink(FsNode* link, FsNode* workingDir){
		assert(link);

		char buffer[PATH_MAX + 1];

		auto bytesRead = link->ReadLink(buffer, PATH_MAX);
		if(bytesRead < 0){
			Log::Warning("FollowLink: Readlink error %d", -bytesRead);
			return nullptr;
		}
		buffer[bytesRead] = 0; // Null terminate

		FsNode* node = ResolvePath(buffer, workingDir, false);

		if(!node){
			Log::Warning("FollowLink: Failed to resolve symlink %s!", buffer);
		}
		return node;
	}

	FsNode* ResolvePath(const char* path, const char* workingDir, bool followSymlinks){
		assert(path);

		char* tempPath;
		if(workingDir && path[0] != '/'){ // If the path starts with '/' then treat as an absolute path
			tempPath = (char*)kmalloc(strlen(path) + strlen(workingDir) + 2);
			strcpy(tempPath, workingDir);
			strcpy(tempPath + strlen(tempPath), "/");
			strcpy(tempPath + strlen(tempPath), path);
		} else {
			tempPath = (char*)kmalloc(strlen(path) + 1);
			strcpy(tempPath, path);
		}

		FsNode* root = fs::GetRoot();
		FsNode* currentNode = root;

		char* file = strtok(tempPath,"/");
		
		while(file != NULL){ // Iterate through the directories to find the file
			FsNode* node = fs::FindDir(currentNode,file);
			if(!node) {
				Log::Warning("%s not found!", file);
				kfree(tempPath);
				return nullptr;
			}

			size_t amountOfSymlinks = 0;
			while(((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK)){ // Check for symlinks
				if(amountOfSymlinks++ > MAXIMUM_SYMLINK_AMOUNT){
					Log::Warning("ResolvePath: Reached maximum number of symlinks");
					return nullptr;
				}

				node = FollowLink(node, currentNode);

				if(!node){
					Log::Warning("ResolvePath: Unresolved symlink!");
					kfree(tempPath);
					return node;
				}
			}

			if((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
				currentNode = node;
				file = strtok(NULL, "/");
				continue;
			}

			if((file = strtok(NULL, "/"))){
				Log::Warning("%s is not a directory!", file);
				kfree(tempPath);
				return nullptr;
			}

			amountOfSymlinks = 0;
			while(followSymlinks && ((currentNode->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK)){ // Check for symlinks
				if(amountOfSymlinks++ > MAXIMUM_SYMLINK_AMOUNT){
					Log::Warning("ResolvePath: Reached maximum number of symlinks");
					return nullptr;
				}

				currentNode = FollowLink(node, currentNode);

				if(!node){
					Log::Warning("ResolvePath: Unresolved symlink!");
					kfree(tempPath);
					return node;
				}
			}

			currentNode = node;
			break;
		}
		kfree(tempPath);
		return currentNode;
	}
	
	FsNode* ResolvePath(const char* path, FsNode* workingDir, bool followSymlinks){
		FsNode* root = fs::GetRoot();
		FsNode* currentNode = root;

		char* tempPath = (char*)kmalloc(strlen(path) + 1);
		strcpy(tempPath, path);

		if(workingDir && path[0] != '/'){
			currentNode = workingDir;
		}

		char* file = strtok(tempPath,"/");

		while(file != NULL){ // Iterate through the directories to find the file
			FsNode* node = fs::FindDir(currentNode,file);
			if(!node) {
				IF_DEBUG((debugLevelFilesystem >= DebugLevelVerbose), {
					Log::Warning("%s not found!", path);
				});

				return nullptr;
			}

			size_t amountOfSymlinks = 0;
			while(followSymlinks && ((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK)){ // Check for symlinks
				if(amountOfSymlinks++ > MAXIMUM_SYMLINK_AMOUNT){
					IF_DEBUG((debugLevelFilesystem >= DebugLevelVerbose), {
						Log::Warning("ResolvePath: Reached maximum number of symlinks");
					});
					return nullptr;
				}

				node = FollowLink(node, currentNode);

				if(!node){
					IF_DEBUG((debugLevelFilesystem >= DebugLevelNormal), {
						Log::Warning("ResolvePath: Unresolved symlink!");
					});
					kfree(tempPath);
					return node;
				}
			}

			if((node->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
				currentNode = node;
				file = strtok(NULL, "/");
				continue;
			}

			if((file = strtok(NULL, "/"))){
				IF_DEBUG((debugLevelFilesystem >= DebugLevelNormal), {
					Log::Warning("%s is not a directory!", file);
				});
				return nullptr;
			}

			amountOfSymlinks = 0;
			while(followSymlinks && ((currentNode->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK)){ // Check for symlinks
				if(amountOfSymlinks++ > MAXIMUM_SYMLINK_AMOUNT){
					IF_DEBUG((debugLevelFilesystem >= DebugLevelVerbose), {
						Log::Warning("ResolvePath: Reached maximum number of symlinks");
					});
					return nullptr;
				}

				currentNode = FollowLink(node, currentNode);

				if(!node){
					IF_DEBUG((debugLevelFilesystem >= DebugLevelNormal), {
						Log::Warning("ResolvePath: Unresolved symlink!");
					});
					kfree(tempPath);
					return currentNode;
				}
			}
			currentNode = node;
			break;
		}

		kfree(tempPath);
		return currentNode;
	}
	
	FsNode* ResolveParent(const char* path, const char* workingDir){
		char pathCopy[strlen(path) + 1];
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
			if(!strlen(pathCopy)){ // Root
				return fs::GetRoot();
			} else {
				parentDirectory = fs::ResolvePath(pathCopy, workingDir);
			}
		}

		return parentDirectory;
	}
	
	FsNode* ResolveParent(const char* path, FsNode* workingDir){
		char* pathCopy = (char*)kmalloc(strlen(path) + 1);
		strcpy(pathCopy, path);

		if(pathCopy[strlen(pathCopy) - 1] == '/'){ // Remove trailing slash
			pathCopy[strlen(pathCopy) - 1] = 0;
		}

		char* dirPath = strrchr(pathCopy, '/');

		FsNode* parentDirectory = nullptr;

		if(dirPath == nullptr){
			parentDirectory = workingDir;
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
		for(unsigned i = 0; i < tokens->get_length(); i++){
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
		else for(unsigned i = 0; i < tokens->get_length(); i++){
			strcpy(outPath + strlen(outPath), "/");
			strcpy(outPath + strlen(outPath), tokens->get_at(i));
		}

		kfree(tempPath);
		delete tokens;

		return outPath;
	}

	char* BaseName(const char* path){
		char* pathCopy = (char*)kmalloc(strlen(path) + 1);
		strcpy(pathCopy, path);
		
		if(pathCopy[strlen(pathCopy) - 1] == '/'){ // Remove trailing slash
			pathCopy[strlen(pathCopy) - 1] = 0;
		}

		char* basename = nullptr;
		char* temp;
		if((temp = strrchr(pathCopy, '/'))){
			basename = (char*)kmalloc(strlen(temp) + 1);
			strcpy(basename, temp);

			kfree(pathCopy);
		} else {
			basename = pathCopy;
		}

		return basename;
	}

	int Root::ReadDir(DirectoryEntry* dirent, uint32_t index){
		if (index < fs::volumes->get_length()){
			*dirent = (volumes->get_at(index)->mountPointDirent);
			return 1;
		} else return 0;
	}

    FsNode* Root::FindDir(char* name){
		if(strcmp(name, ".") == 0) return this;
		if(strcmp(name, "..") == 0) return this;

		for(unsigned i = 0; i < fs::volumes->get_length(); i++){
			if(strcmp(fs::volumes->get_at(i)->mountPointDirent.name,name) == 0) return (fs::volumes->get_at(i)->mountPointDirent.node);
		}

        return NULL;
	}

    ssize_t Read(FsNode* node, size_t offset, size_t size, uint8_t *buffer){
		assert(node);

        return node->Read(offset,size,buffer);
    }

    ssize_t Write(FsNode* node, size_t offset, size_t size, uint8_t *buffer){
		assert(node);

        return node->Write(offset,size,buffer);
    }

    fs_fd_t* Open(FsNode* node, uint32_t flags){
		/*if((node->flags & S_IFMT) == S_IFLNK){
			char pathBuffer[PATH_MAX];

			ssize_t bytesRead = node->ReadLink(pathBuffer, PATH_MAX);
			if(bytesRead < 0){
				Log::Warning("fs::Open: Readlink error");
				return nullptr;
			}
			pathBuffer[bytesRead] = 0; // Null terminate

			FsNode* link = fs::ResolvePath(pathBuffer);
			if(!link){
				Log::Warning("fs::Open: Invalid symbolic link");
			}
			return link->Open(flags);
		}*/

        return node->Open(flags);
    }
	
    int Link(FsNode* dir, FsNode* link, DirectoryEntry* ent){
		assert(dir);
		assert(link);

		return dir->Link(link, ent);
	}

    int Unlink(FsNode* dir, DirectoryEntry* ent, bool unlinkDirectories){
		assert(dir);
		assert(ent);

		return dir->Unlink(ent, unlinkDirectories);
	}

    void Close(FsNode* node){
        return node->Close();
    }

    void Close(fs_fd_t* fd){
		if(!fd) return;

        fd->node->Close();
		fd->node = nullptr;

		kfree(fd);
    }

    int ReadDir(FsNode* node, DirectoryEntry* dirent, uint32_t index){
		assert(node);

		//if((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK) return ReadDir(node->link, dirent, index);

        return node->ReadDir(dirent, index);
    }

    FsNode* FindDir(FsNode* node, char* name){
		assert(node);

		//if((node->flags & FS_NODE_TYPE) == FS_NODE_SYMLINK) return FindDir(node->link, name);
            
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
		} else return -1;
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

	int Rename(FsNode* olddir, char* oldpath, FsNode* newdir, char* newpath){
		assert(olddir && newdir);

		if((olddir->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
			return -ENOTDIR;
		}
		
		if((newdir->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
			return -ENOTDIR;
		}

		FsNode* oldnode = ResolvePath(oldpath, olddir);

		if(!oldnode){
			return -ENOENT;
		}

		if((oldnode->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
			Log::Warning("Filesystem: Rename: We do not support using rename on directories yet!");
			return -ENOSYS;
		}
		
		FsNode* newpathParent = ResolveParent(newpath, newdir); 

		if(!newpathParent){
			return -ENOENT;
		} else if((newpathParent->flags & FS_NODE_TYPE) != FS_NODE_DIRECTORY){
				return -ENOTDIR; // Parent of newpath is not a directory
		}

		FsNode* newnode = ResolvePath(newpath, newdir);

		if(newnode){
			if((newnode->flags & FS_NODE_TYPE) == FS_NODE_DIRECTORY){
				return -EISDIR; // If it exists newpath must not be a directory
			}
		}

		DirectoryEntry oldpathDirent;
		strncpy(oldpathDirent.name, fs::BaseName(oldpath), NAME_MAX);

		DirectoryEntry newpathDirent;
		strncpy(newpathDirent.name, fs::BaseName(newpath), NAME_MAX);

		if((oldnode->flags & FS_NODE_TYPE) != FS_NODE_SYMLINK && oldnode->volumeID == newpathParent->volumeID){ // Easy shit we can just link and unlink
			FsNode* oldpathParent = fs::ResolveParent(oldpath, olddir);
			assert(oldpathParent); // If this is null something went horribly wrong

			if(newnode){
				if(auto e = newpathParent->Unlink(&newpathDirent)){
					return e; // Unlink error
				}
			}

			if(auto e = newpathParent->Link(oldnode, &newpathDirent)){
				return e; // Link error
			}
			
			if(auto e = oldpathParent->Unlink(&oldpathDirent)){
				return e; // Unlink error
			}
		} else if((oldnode->flags & FS_NODE_TYPE) != FS_NODE_SYMLINK) { // Aight we have to copy it
			FsNode* oldpathParent = fs::ResolveParent(oldpath, olddir);
			assert(oldpathParent); // If this is null something went horribly wrong

			if(auto e = newpathParent->Create(&newpathDirent, 0)){
				return e; // Create error
			}

			newnode = ResolvePath(newpath, newdir);
			if(!newnode){
				Log::Warning("Filesystem: Rename: newpath was created with no error returned however it was unable to be found.");
				return -ENOENT;
			}

			uint8_t* buffer = (uint8_t*)kmalloc(oldnode->size);

			ssize_t rret = oldnode->Read(0, oldnode->size, buffer);
			if(rret < 0){
				Log::Warning("Filesystem: Rename: Error reading oldpath");
				return rret;
			}

			ssize_t wret = oldnode->Write(0, rret, buffer);
			if(wret < 0){
				Log::Warning("Filesystem: Rename: Error reading oldpath");
				return wret;
			}
			
			if(auto e = oldpathParent->Unlink(&oldpathDirent)){
				return e; // Unlink error
			}
		} else {
			Log::Warning("Filesystem: Rename: We do not support using rename on symlinks yet!"); // TODO: Rename the symlink
			return -ENOSYS;
		}

		return 0;
	}
}