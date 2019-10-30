#include <core/syscall.h>
#include <core/filesystem.h>
#include <string.h>

#include "stdio_internal.h"
#include <stdio.h>

FILE* fdopen(int fd, const char* mode){
    FILE* file;
    if(!(file = malloc(sizeof(struct IOFILE) + BUFSIZE))) return 0; // Attempt to allocate FILE

    file->fd = fd;
    file->buf = file + sizeof(struct IOFILE);
    file->buf_size = BUFSIZE;

    return file;
}

FILE* fopen(const char* filename, const char* mode){
    FILE *file;
	int fd;
	int flags = 0;

	fd = lemon_open(filename, flags);
	if (fd < 0) return 0;

	file = fdopen(fd, mode);
	if (file) return file;

	lemon_close(fd);
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
	int ret = lemon_read(fd, buffer, size*count);
	return ret;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE* stream){
	if(!buffer) return;

	int fd = stream->fd;

	int ret = lemon_write(fd, buffer, count*size);

	return ret;
}

int fseek(FILE* stream, long int offset, int whence){
	return lemon_seek(stream->fd, offset, whence);
}

long ftell(FILE* stream){
	return lemon_seek(stream->fd, 0, SEEK_END);
}

int fflush(FILE* stream){
	return 0; // We currently dont buffer anything
}

int feof(FILE* f){
	return 0;
}

int fputc(int c, FILE* f){
	return fwrite(&c, 1, 1, c);
}

int fputs(const char* str, FILE* f)
{
	int length = strlen(str);
	return fwrite(str, length, 1, f) == length ? 0 : -1;
}