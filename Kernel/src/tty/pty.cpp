#include <filesystem.h>
#include <string.h>
#include <pty.h>
#include <list.h>
#include <types.h>
#include <logging.h>

char nextPTY = '0';

char GetNextPTY(){
	nextPTY++;
	if(nextPTY < 'a' && nextPTY > '9') nextPTY = 'a';
	else if (nextPTY < 'A' && nextPTY > 'z') nextPTY = 'A';
}

List<PTY*>* ptys = NULL;

PTY* GrantPTY(uint64_t pid){
	if(!ptys) ptys = new List<PTY*>();
	Log::Info(ptys->get_length());

	PTY* pty = new PTY();
	
	return pty;
}

size_t FS_Slave_Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	return pty->Slave_Read((char*)buffer,size);
}

size_t FS_Master_Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	size_t ret = pty->Master_Read((char*)buffer,size);
	return ret;
}
	
size_t FS_Slave_Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	return pty->Slave_Write((char*)buffer,size);
}

size_t FS_Master_Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
	Log::Info("Master Write");
	PTY* pty = (*ptys)[node->inode];
	Log::Info("write: ");
	Log::Error((char*)(buffer + 1));
	Log::Info(*(buffer + 1),true);
	return pty->Master_Write((char*)(buffer + 1),size);
}

PTY::PTY(){
	Log::Warning("test2");
	slaveFile.flags = FS_NODE_CHARDEVICE;
	Log::Warning("test2");
	strcpy(slaveFile.name, "pty");
	char _name[] = {nextPTY, 0};
	strcpy(slaveFile.name + strlen(slaveFile.name), _name);
	Log::Warning("test1");
	GetNextPTY();

	slaveFile.read = FS_Slave_Read;
	slaveFile.write = FS_Slave_Write;
	slaveFile.findDir = nullptr;
	slaveFile.readDir = nullptr;
	slaveFile.open = nullptr;
	slaveFile.close = nullptr;
	slaveFile.inode = ptys->get_length();

	masterFile.read = FS_Master_Read;
	masterFile.write = FS_Master_Write;
	masterFile.findDir = nullptr;
	masterFile.readDir = nullptr;
	masterFile.open = nullptr;
	masterFile.close = nullptr;
	masterFile.inode = ptys->get_length();

	fs::RegisterDevice(&slaveFile);

	ptys->add_back(this);
}

size_t PTY::Master_Read(char* buffer, size_t count){
	return master.Read(buffer, count);
}

size_t PTY::Slave_Read(char* buffer, size_t count){
	while(slave.bufferPos < count) ;//Log::Write("waiting");

	Log::Info("reading from slave");

	return slave.Read(buffer, count);
}

size_t PTY::Master_Write(char* buffer, size_t count){
	return slave.Write(buffer, count);
}

size_t PTY::Slave_Write(char* buffer, size_t count){
	return master.Write(buffer, count);
}