#include <fs/fsvolume.h>

namespace fs{
	FsNode* FsVolume::AllocateNode(){
		return nullptr;
	}

	int FsVolume::WriteNode(FsNode* node){
		return -1;
	}
}