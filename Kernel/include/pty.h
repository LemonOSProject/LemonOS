#pragma once

#include <characterbuffer.h>
#include <types.h>

#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

struct winsz {
	unsigned short rowCount; // Rows (in characters)
	unsigned short colCount; // Columns (in characters)
	unsigned short width; // Width (in pixels)
	unsigned short height; // Height (in pixels)
};

class PTY{
public:
    CharacterBuffer master;
    CharacterBuffer slave;

    fs_node_t masterFile;
    fs_node_t slaveFile;

    winsz wSz;

    bool canonical = true;
    bool echo = true;

    PTY();
    
    void UpdateLineCount();

    size_t Master_Read(char* buffer, size_t count);
    size_t Slave_Read(char* buffer, size_t count);

    size_t Master_Write(char* buffer, size_t count);
    size_t Slave_Write(char* buffer, size_t count);
};

PTY* GrantPTY(uint64_t pid);