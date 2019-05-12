#include <core/syscall.h>
#include <core/filesystem.h>
#include <stdio.h>

#include "stdio_internal.h"

FILE* fdopen(int fd, const char* mode){
    FILE* file;
    if(!(file = malloc(sizeof(FILE) + BUFSIZE))) return 0; // Attempt to allocate FILE

    file->fd = fd;
    file->buf = file + sizeof(FILE);
    file->buf_size = BUFSIZE;

    return file;
}

FILE* fopen(const char* filename, const char* mode){
    FILE *file;
	int fd;
	int flags;

	fd = lemon_open(filename, flags);
	if (fd < 0) return 0;

	file = fdopen(fd, mode);
	if (file) return file;

	//lemon_close(fd);
	return 0;
}

int fclose(FILE* file){
	int fd = file->fd;
	lemon_close(fd);
	return 0;
	#warning "Finish return on fclose"
}

size_t fread(void* buffer, size_t size, size_t count, FILE* stream){
	int fd = stream->fd;
	lemon_read(fd, buffer, size*count);
	return count;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE* stream){
	if(!buffer) return;

	int fd = stream->fd;

	lemon_write(fd, buffer, count*size);

	return count;
}

int fseek(FILE* stream, long int offset, int whence){
	
}

int fflush(FILE* stream){
	return 0; // We currently dont buffer anything
}