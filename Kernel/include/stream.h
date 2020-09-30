#pragma once

#include <list.h>

#include <stdint.h>
#include <stddef.h>
#include <lock.h>
#include <scheduler.h>

#define DATASTREAM_BUFSIZE_DEFAULT 1024

typedef struct {
    uint8_t* data;
    size_t len;
} stream_packet_t;

class Stream {
protected:
    List<thread_t*> waiting;

public:
    virtual void Wait();

    virtual int64_t Read(void* buffer, size_t len);
    virtual int64_t Peek(void* buffer, size_t len);
    virtual int64_t Write(void* buffer, size_t len);

    virtual int64_t Pos() { return 0; }
    virtual int64_t Empty();

    virtual ~Stream();
};

class DataStream final : public Stream {
    lock_t streamLock = 0;

    size_t bufferSize = 0;
    size_t bufferPos = 0;

    uint8_t* buffer = nullptr;
public:
    DataStream(size_t bufSize);
    ~DataStream();

    void Wait();

    int64_t Read(void* buffer, size_t len);
    int64_t Peek(void* buffer, size_t len);
    int64_t Write(void* buffer, size_t len);
    
    int64_t Pos() { return bufferPos; }
    virtual int64_t Empty();
};

class PacketStream final : public Stream {
    List<stream_packet_t> packets;
public:
    void Wait();

    int64_t Read(void* buffer, size_t len);
    int64_t Peek(void* buffer, size_t len);
    int64_t Write(void* buffer, size_t len);
    virtual int64_t Empty();
};