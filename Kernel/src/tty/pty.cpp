#include <filesystem.h>
#include <string.h>
#include <pty.h>
#include <list.h>
#include <initrd.h>
#include <types.h>
#include <logging.h>

char nextPTY = '0';

char GetNextPTY(){
	if(nextPTY < 'a' && nextPTY > '9') nextPTY = 'a';
	else if (nextPTY < 'A' && nextPTY > 'z') nextPTY = 'A';
}

List<PTY*>* ptys = NULL;

PTY* GrantPTY(pid_t pid){
	if(!ptys) ptys = new List<PTY*>();
	Log::Info(ptys->get_length());

	PTY* pty = new PTY();
	
	return pty;
}

uint32_t FS_Slave_Read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	return pty->Slave_Read((char*)buffer,size);
}

uint32_t FS_Master_Read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	uint32_t ret = pty->Master_Read((char*)buffer,size);
	Log::Info("reading");
	if(ret > 0){
		Log::Info("Returning with:");
		Log::Info(ret,false);
	}
	return ret;
}
	
uint32_t FS_Slave_Write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	return pty->Slave_Write((char*)buffer,size);
}

uint32_t FS_Master_Write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	return pty->Master_Write((char*)buffer,size);
}

PTY::PTY(){
	slaveFile.flags = FS_NODE_CHARDEVICE;

	strcpy(slaveFile.name, &nextPTY);
	GetNextPTY();

	slaveFile.read = FS_Slave_Read;
	slaveFile.write = FS_Slave_Write;
	slaveFile.inode = ptys->get_length();

	masterFile.read = FS_Master_Read;
	masterFile.write = FS_Master_Write;
	masterFile.inode = ptys->get_length();

	Initrd::RegisterDevice(&slaveFile);

	ptys->add_back(this);
}

size_t PTY::Master_Read(char* buffer, size_t count){
	return master.Read(buffer, count);
}

size_t PTY::Slave_Read(char* buffer, size_t count){
	return slave.Read(buffer, count);
}

size_t PTY::Master_Write(char* buffer, size_t count){
	return slave.Write(buffer, count);
}

size_t PTY::Slave_Write(char* buffer, size_t count){
	return master.Write(buffer, count);
}