#include <filesystem.h>
#include <string.h>
#include <liballoc.h>
#include <list.h>

void *operator new(size_t size)
{
	return kmalloc(size);
}

void operator delete(void *p)
{
	kfree(p);
}

class PTY{ 
public:
    fs_node_t* node;
    char buffer[128];
    int bufferPos = 0;
    int bufferSize = 128;

    void Read(long int count, char* _buffer){
        memcpy(_buffer, buffer, (count < bufferPos) ? count : bufferPos);
    }

    void Write(long int count, char* _buffer){
        if(bufferPos + count >= bufferSize){
            krealloc(buffer, bufferSize + 32);
            bufferSize += 32;
        }
        memcpy((buffer + bufferPos), _buffer, count);
        bufferPos += count;
    }

    ~PTY(){
        kfree(buffer);
    }
};

char nextPTY = 'A';
List<PTY*> ptys;

fs_node_t* NewPTY(){
    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    PTY* pty = new PTY();
    pty->node = node;

    node->name = nextPTY;

    ptys.add_back(pty);

    return node;
}

void DestroyPTY(){

}