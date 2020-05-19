#include <fs/filesystem.h>
#include <string.h>
#include <pty.h>
#include <list.h>
#include <types.h>
#include <logging.h>
#include <scheduler.h>
#include <assert.h>

cc_t c_cc_default[NCCS]{
	4,			// VEOF
	'\n',		// VEOL
	'\b',			// VERASE
	0,			// VINTR
	0,			// VKILL
	0,			// VMIN
	0,			// VQUIT
	0,			// VSTART
	0,			// VSTOP
	0,			// VSUSP
	0,			// VTIME
};

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

size_t PTYDevice::Read(size_t offset, size_t size, uint8_t *buffer){
	assert(pty);
	assert(device == PTYSlaveDevice || device == PTYMasterDevice);

	if(pty && device == PTYSlaveDevice)
		return pty->Slave_Read((char*)buffer,size);
	else if(pty && device == PTYMasterDevice){
		return pty->Master_Read((char*)buffer,size);
	}
}
	
size_t PTYDevice::Write(size_t offset, size_t size, uint8_t *buffer){
	assert(pty);
	assert(device == PTYSlaveDevice || device == PTYMasterDevice);

	if(pty && device == PTYSlaveDevice)
		return pty->Slave_Write((char*)buffer,size);
	else if(pty && device == PTYMasterDevice){
		return pty->Master_Write((char*)buffer,size);
	}
}

int PTYDevice::Ioctl(uint64_t cmd, uint64_t arg){
	assert(pty);

	switch(cmd){
		case TIOCGWINSZ:
			*((winsz*)arg) = pty->wSz;
			break;
		case TIOCSWINSZ:
			pty->wSz = *((winsz*)arg);
			break;
		case TIOCGATTR:
			*((termios*)arg) = pty->tios;
			break;
		case TIOCSATTR:
			pty->tios = *((termios*)arg);
			break;
		case TIOCFLUSH:
			if(arg == TCIFLUSH || arg == TCIOFLUSH){
				pty->slave.Flush();
			}
			if(arg == TCOFLUSH || arg == TCIOFLUSH){
				pty->master.Flush();
			}
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
	master.Flush();
	slave.Flush();
	canonical = true;

	masterFile.pty = this;
	masterFile.device = PTYMasterDevice;

	slaveFile.pty = this;
	slaveFile.device = PTYSlaveDevice;

	for(int i = 0; i < NCCS; i++) tios.c_cc[i] = c_cc_default[i];

	fs::RegisterDevice(&slaveFile);

	ptys->add_back(this);
}

size_t PTY::Master_Read(char* buffer, size_t count){
	return master.Read(buffer, count);
}

size_t PTY::Slave_Read(char* buffer, size_t count){
	while(canonical && !slave.lines) Scheduler::Yield();

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