#include <fs/filesystem.h>
#include <string.h>
#include <pty.h>
#include <list.h>
#include <types.h>
#include <logging.h>
#include <scheduler.h>
#include <assert.h>
#include <cpu.h>
#include <errno.h>

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
	
	return nextPTY;
}

List<PTY*>* ptys = NULL;

PTY* GrantPTY(uint64_t pid){
	if(!ptys) ptys = new List<PTY*>();

	char name[5] = {'p', 't', 'y', 0, 0};
	name[4] = nextPTY;
	GetNextPTY();

	PTY* pty = new PTY(name);
	
	return pty;
}

PTYDevice::PTYDevice(const char* name) : Device(name, TypePseudoterminalDevice){
	dirent.node = this;
}

ssize_t PTYDevice::Read(size_t offset, size_t size, uint8_t *buffer){
	assert(pty);
	assert(device == PTYSlaveDevice || device == PTYMasterDevice);

	if(pty && device == PTYSlaveDevice)
		return pty->Slave_Read((char*)buffer,size);
	else if(pty && device == PTYMasterDevice){
		return pty->Master_Read((char*)buffer,size);
	} else {
		assert(!"PTYDevice::Read: PTYDevice is designated neither slave nor master");
	}

	return 0;
}
	
ssize_t PTYDevice::Write(size_t offset, size_t size, uint8_t *buffer){
	assert(pty);
	assert(device == PTYSlaveDevice || device == PTYMasterDevice);

	if(pty && device == PTYSlaveDevice)
		return pty->Slave_Write((char*)buffer,size);
	else if(pty && device == PTYMasterDevice){
		return pty->Master_Write((char*)buffer,size);
	} else {
		assert(!"PTYDevice::Write: PTYDevice is designated neither slave nor master");
	}

	return 0;
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
			pty->slave.ignoreBackspace = !pty->IsCanonical();
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

void PTYDevice::Watch(FilesystemWatcher& watcher, int events){
	if(device == PTYMasterDevice){
		pty->WatchMaster(watcher, events);
	} else if(device == PTYSlaveDevice) {
		pty->WatchSlave(watcher, events);
	} else {
		assert(!"PTYDevice::Watch: PTYDevice is designated neither slave nor master");
	}
}

void PTYDevice::Unwatch(FilesystemWatcher& watcher){
	if(device == PTYMasterDevice){
		pty->UnwatchMaster(watcher);
	} else if(device == PTYSlaveDevice) {
		pty->UnwatchSlave(watcher);
	} else {
		assert(!"PTYDevice::Unwatch: PTYDevice is designated neither slave nor master");
	}
}

bool PTYDevice::CanRead() {
	if(device == PTYMasterDevice){
		return !!pty->master.bufferPos;
	} else if(device == PTYSlaveDevice){
		if(pty->IsCanonical())
			return !!pty->slave.lines;
		else 
			return !!pty->slave.bufferPos;
	} else {
		assert(!"PTYDevice::CanRead: PTYDevice is designated neither slave nor master");
	}
}

PTY::PTY(const char* name) : masterFile(name), slaveFile(name){
	slaveFile.flags = FS_NODE_CHARDEVICE;

	master.ignoreBackspace = true;
	slave.ignoreBackspace = false;
	master.Flush();
	slave.Flush();
	tios.c_lflag = ECHO | ICANON;

	masterFile.pty = this;
	masterFile.device = PTYMasterDevice;

	slaveFile.pty = this;
	slaveFile.device = PTYSlaveDevice;

	for(int i = 0; i < NCCS; i++) tios.c_cc[i] = c_cc_default[i];

	DeviceManager::RegisterDevice(slaveFile);

	ptys->add_back(this);
}

ssize_t PTY::Master_Read(char* buffer, size_t count){
	return master.Read(buffer, count);
}

ssize_t PTY::Slave_Read(char* buffer, size_t count){
	Thread* thread = GetCPULocal()->currentThread;

	while(IsCanonical() && !slave.lines){
		FilesystemBlocker blocker(&slaveFile);

		if(thread->Block(&blocker)){
			return -EINTR;
		}
	}

	while(!IsCanonical() && !slave.bufferPos){
		FilesystemBlocker blocker(&slaveFile);

		if(thread->Block(&blocker)){
			return -EINTR;
		}
	}

	return slave.Read(buffer, count);
}

ssize_t PTY::Master_Write(char* buffer, size_t count){
	ssize_t ret = slave.Write(buffer, count);

	if(slaveFile.blocked.get_length()){
		if(IsCanonical()){
			acquireLock(&slaveFile.blockedLock);
			while(slave.lines && slaveFile.blocked.get_length()){
				slaveFile.blocked.remove_at(0)->Unblock();
			}
			releaseLock(&slaveFile.blockedLock);
		} else {
			acquireLock(&slaveFile.blockedLock);
			while(slave.bufferPos && slaveFile.blocked.get_length()){
				slaveFile.blocked.remove_at(0)->Unblock();
				Log::Info("unblocked!");
			}
			releaseLock(&slaveFile.blockedLock);
		}
	}

	if(Echo() && ret){
		for(unsigned i = 0; i < count; i++){
			if(buffer[i] == '\e'){ // Escape
				master.Write("^[", 2);
			} else {
				master.Write(&buffer[i], 1);
			}
		}
	}

	if(IsCanonical()){
		if(slave.lines && watchingSlave.get_length()){
			while(watchingSlave.get_length()){
				watchingSlave.remove_at(0)->Signal(); // Signal all watching
			}
		}
	} else {
		if(slave.bufferPos && watchingSlave.get_length()){
			while(watchingSlave.get_length()){
				watchingSlave.remove_at(0)->Signal(); // Signal all watching
			}
		}
	}

	return ret;
}

ssize_t PTY::Slave_Write(char* buffer, size_t count){
	ssize_t written = master.Write(buffer, count);

	acquireLock(&masterFile.blockedLock);
	while(master.bufferPos && masterFile.blocked.get_length()){
		masterFile.blocked.remove_at(0)->Unblock();
	}
	releaseLock(&masterFile.blockedLock);

	if(master.bufferPos && watchingMaster.get_length()){
		while(watchingMaster.get_length()){
			watchingMaster.remove_at(0)->Signal(); // Signal all watching
		}
	}
	
	return written;
}

void PTY::WatchMaster(FilesystemWatcher& watcher, int events){
	if(!(events & (POLLIN))){ // We don't really block on writes and nothing else applies except POLLIN
		watcher.Signal();
		return;
	} else if(masterFile.CanRead()){
		watcher.Signal();
		return;
	}

	watchingMaster.add_back(&watcher);
}

void PTY::WatchSlave(FilesystemWatcher& watcher, int events){
	if(!(events & (POLLIN))){ // We don't really block on writes and nothing else applies except POLLIN
		watcher.Signal();
		return;
	} else if(slaveFile.CanRead()){
		watcher.Signal();
		return;
	}

	watchingSlave.add_back(&watcher);
}

void PTY::UnwatchMaster(FilesystemWatcher& watcher){
	watchingMaster.remove(&watcher);
}

void PTY::UnwatchSlave(FilesystemWatcher& watcher){
	watchingSlave.remove(&watcher);
}