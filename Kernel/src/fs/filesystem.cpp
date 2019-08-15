#include <filesystem.h>

namespace fs{
    fs_node_t root;

    uint32_t Read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
        if(node->read)
            return node->read(node,offset,size,buffer);
        else return 0;
    }

    uint32_t Write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
        if(node->write)
            return node->write(node,offset,size,buffer);
        else return 0;
    }

    void Open(fs_node_t* node, uint32_t flags){
        if(node->write)
            return node->open(node,flags);
    }

    void Close(fs_node_t* node){
        if(node->write)
            return node->close(node);
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

    void Initialize(){
        
    }
}