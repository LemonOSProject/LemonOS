#include <iostream>
#include <fstream>
#include <stdint.h>
#include <cstring>

#define LEMONINITFS_FILE        0x01
#define LEMONINITFS_DIRECTORY   0x02
#define LEMONINITFS_SYMLINK     0x03
#define LEMONINITFS_MOUNTPOINT  0x04

#define MAX_FILES 256

using namespace std;

char version[16] = {'l','e','m','o','n','i','n','i','t','f','s','v','1',0,0,0};

typedef struct {
	uint32_t num_files = 0; // Amount of files present
	char versionstring[16]; // Version string
} __attribute__((packed)) lemoninitfs_header_t;

typedef struct {
	uint16_t magic = 0; // Check for corruption (should be 0xBEEF)
	char filename[32]; // Filename
	uint32_t offset = 0; // Offset in file
	uint32_t size = 0; // File Size
	uint8_t type = 0;
} __attribute__((packed)) lemoninitfs_node_t ;

int main(int argc, char** argv){
    if(argc < 4){
        cout << "Usage: " << argv[0] << " <output file> file1 file1_name [<file2> <file2name>] ...";
        return 1;
    }
    
    ofstream output;
    output.open(argv[1], ios::binary);
    if(!output.is_open()){
        cout << "Could not open file " << argv[1] << " for writing";
        return 2;
    }

    lemoninitfs_header_t initrd_header;
    strcpy(initrd_header.versionstring,version); // Copy the version string
    initrd_header.num_files = argc / 2 - 1; // Get number of files (first two arguments are the command and output file)

    output.write((char*)&initrd_header,sizeof(lemoninitfs_header_t));

    lemoninitfs_node_t* initrd_nodes = new lemoninitfs_node_t[256];

    fstream input;

    uint32_t offset = sizeof(lemoninitfs_node_t) * 256 + sizeof(lemoninitfs_header_t);

    for(int i = 0; i < argc - 2; i += 2){
        lemoninitfs_node_t* node = &initrd_nodes[i / 2];

        input.open(argv[i + 2]);
        input.seekg(0,ios::beg);
        uint32_t start = input.tellg();
        input.seekg(0,ios::end);
        
        uint32_t size = input.tellg();
        size -= start;

        input.seekg(0,ios::beg);
        input.close();
        strcpy(node->filename,argv[i + 3]);
        node->magic = 0xBEEF;
        node->type = LEMONINITFS_FILE;
        node->size = size;
        node->offset = offset;

        offset += size;
    }

    output.seekp(sizeof(lemoninitfs_header_t));
    output.write((char*)initrd_nodes, sizeof(lemoninitfs_node_t) * 256);

    for(int i = 0; i < argc - 2; i += 2){
        input.open(argv[i + 2]);

        if(!input.is_open()){
            cout << "Could not open file " << argv[i + 2] << " for reading" << endl;
            return 1;
        }

        input.seekg(0,ios::beg);
        uint32_t start = input.tellg();
        input.seekg(0,ios::end);
        
        uint32_t size = input.tellg();
        size -= start;

        input.seekg(0,ios::beg);

        cout << "Writing file at " << argv[i + 3] << " with name " << argv[i + 2] << " and size " << (size) << "B" << endl;

        char* data = new char[size];

        input.read(data,size);
        input.close();
        output.seekp(initrd_nodes[i/2].offset);
        output.write(data, size);
        delete data;
    }
}