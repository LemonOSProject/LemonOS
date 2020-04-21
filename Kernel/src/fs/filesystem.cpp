#include <filesystem.h>

#include <fsvolume.h>
#include <logging.h>

namespace fs{
    size_t ReadNull(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer);

    fs_dirent_t* RootReadDir(fs_node_t* node, uint32_t index);
    fs_node_t* RootFindDir(fs_node_t* node, char* name);
    fs_dirent_t* DevReadDir(fs_node_t* node, uint32_t index);
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
		null.flags = FS_NODE_FILE;
		null.inode = 0;

		null.read = ReadNull;

        RegisterDevice(&null);
    }

    fs_node_t* GetRoot(){
        return &root;
    }

	void RegisterDevice(fs_node_t* device){
		Log::Info("Device Registered: ");
		Log::Write(device->name);
		devices[deviceCount++] = device;
	}
    
	fs_dirent_t* RootReadDir(fs_node_t* node, uint32_t index){
		if(node == &root && index == 0){
			return &devDirent;
		} else if (node == &root && index < fs::volumes->get_length() + 1){
			return &(volumes->get_at(index - 1)->mountPointDirent);
		} else return nullptr;
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
    
	fs_dirent_t* DevReadDir(fs_node_t* node, uint32_t index){
		if(node == &dev){
			if(index >= deviceCount) return nullptr;
			memset(dirent.name,0,128); // Zero the string
			strcpy(dirent.name,devices[index]->name);
			dirent.inode = index;
			dirent.type = 0;
			return &dirent;
		} else return nullptr;
	}

    fs_node_t* DevFindDir(fs_node_t* node, char* name){
		if(node == &dev){
			for(int i = 0; i < deviceCount; i++)
				if(strcmp(devices[i]->name, name) == 0) return devices[i];
		}

        return NULL;
	}

	size_t ReadNull(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
		// /if(offset > inode.length || offset + size > inode.length) return 0;

		memset(buffer, -1, size);
		return size;
	}

    size_t Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
        if(node->read)
            return node->read(node,offset,size,buffer);
        else return 0;
    }

    size_t Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
        if(node->write)
            return node->write(node,offset,size,buffer);
        else return 0;
    }

    void Open(fs_node_t* node, uint32_t flags){
        if(node->open){
            node->offset = 0;
            return node->open(node,flags);
        }
    }

    void Close(fs_node_t* node){
        if(node->close){
            node->offset = 0;
            return node->close(node);
        }
    }

    fs_dirent_t* ReadDir(fs_node_t* node, uint32_t index){
        if(node->readDir)
            return node->readDir(node,index);
        else return 0;
    }

    fs_node_t* FindDir(fs_node_t* node, char* name){
        if(node->findDir)
            return node->findDir(node,name);
        else return 0;
    }
}