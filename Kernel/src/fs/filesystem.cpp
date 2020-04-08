#include <filesystem.h>

namespace fs{
    fs_node_t root;

	List<FsVolume*>* volumes;

    void Initialize(){
		volumes = new List<FsVolume*>();
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