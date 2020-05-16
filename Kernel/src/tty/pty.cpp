#include <fs/filesystem.h>
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
	if(pty)
		return pty->Slave_Read((char*)buffer,size);
	else return 0;
}

size_t FS_Master_Read(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	if(pty)
		return pty->Master_Read((char*)buffer,size);
	else return 0;
}
	
size_t FS_Slave_Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	if(pty)
		return pty->Slave_Write((char*)buffer,size);
	else return 0;
}

size_t FS_Master_Write(fs_node_t* node, size_t offset, size_t size, uint8_t *buffer){
	PTY* pty = (*ptys)[node->inode];
	if(pty)
		return pty->Master_Write((char*)(buffer + 1),size);
	else return 0;
}

int FS_Ioctl(fs_node_t* node, uint64_t cmd, uint64_t arg){
	PTY* pty = (*ptys)[node->inode];

	if(!pty) return -1;

	switch(cmd){
		case TIOCGWINSZ:
			*((winsz*)arg) = pty->wSz;
			break;
		case TIOCSWINSZ:
			pty->wSz = *((winsz*)arg);
			break;
		default:
			return -1;
	}

	return 0;
}

PTY::PTY(){
	slaveFile.flags = FS_NODE_CHARDEVICE;
	strcpy(slaveFile.name, "pty");
	char _name[] = {nextPTY, 0};
	strcpy(slaveFile.name + strlen(slaveFile.name), _name);
	GetNextPTY();

	master.ignoreBackspace = true;
	slave.ignoreBackspace = false;
	canonical = true;

	slaveFile.read = FS_Slave_Read;
	slaveFile.write = FS_Slave_Write;
	slaveFile.findDir = nullptr;
	slaveFile.readDir = nullptr;
	slaveFile.open = nullptr;
	slaveFile.close = nullptr;
	slaveFile.inode = ptys->get_length();
	slaveFile.ioctl = FS_Ioctl;

	masterFile.read = FS_Master_Read;
	masterFile.write = FS_Master_Write;
	masterFile.findDir = nullptr;
	masterFile.readDir = nullptr;
	masterFile.open = nullptr;
	masterFile.close = nullptr;
	masterFile.inode = ptys->get_length();
	masterFile.ioctl = FS_Ioctl;

	fs::RegisterDevice(&slaveFile);

	ptys->add_back(this);
}

size_t PTY::Master_Read(char* buffer, size_t count){
	return master.Read(buffer, count);
}

size_t PTY::Slave_Read(char* buffer, size_t count){
	while(slave.bufferPos < count) ;

	while(canonical && !slave.lines);

	return slave.Read(buffer, count);
}

size_t PTY::Master_Write(char* buffer, size_t count){
	if(echo)
		master.Write(buffer, count);

	return slave.Write(buffer, count);
}

size_t PTY::Slave_Write(char* buffer, size_t count){
	return master.Write(buffer, count);
}