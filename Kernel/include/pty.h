#pragma once

#include <characterbuffer.h>
#include <types.h>

class PTY{
public:
    CharacterBuffer master;
    CharacterBuffer slave;

    fs_node_t masterFile;
    fs_node_t slaveFile;

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