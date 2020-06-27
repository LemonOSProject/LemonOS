#include <fs/tmp.h>

namespace fs::Temp{
    TempVolume::TempVolume(const char* name){
        strcpy(mountPointDirent.name, name);
        mountPointDirent.node = 
    }

    void TempVolume::ReallocateNode(TempNode* node){
        if(node->size > node->bufferSize){
            if(!node->buffer){
                node->buffer = (uint8_t*)kmalloc(node->size);
            }
        }
    }

    TempNode::TempNode(TempVolume* v){
        vol = v;
    }
}