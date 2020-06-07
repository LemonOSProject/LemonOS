#pragma once

#include <list.h>

#include <stdint.h>
#include <stddef.h>
#include <lock.h>

#define DATASTREAM_BUFSIZE_DEFAULT 1024

typedef struct {
    void* data;
    size_t len;
} stream_packet_t;

class Stream {
public:
    virtual int64_t Read(void* buffer, size_t len);
    virtual int64_t Peek(void* buffer, size_t len);
    virtual int64_t Write(void* buffer, size_t len);
    virtual int64_t Empty();

    virtual ~Stream();
};

class DataStream : public Stream {
    lock_t streamLock = 0;

    size_t bufferSize = 0;
    size_t bufferPos = 0;

    void* buffer = nullptr;
public:
    DataStream(size_t bufSize);
    ~DataStream();

    int64_t Read(void* buffer, size_t len);
    int64_t Peek(void* buffer, size_t len);
    int64_t Write(void* buffer, size_t len);
    virtual int64_t Empty();
};

class PacketStream : public Stream {
    List<stream_packet_t> packets;
public:
    int64_t Read(void* buffer, size_t len);
    int64_t Peek(void* buffer, size_t len);
    int64_t Write(void* buffer, size_t len);
    virtual int64_t Empty();
};