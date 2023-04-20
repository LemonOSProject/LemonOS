#pragma once

#include <List.h>

#include <stdint.h>
#include <stddef.h>
#include <Lock.h>
#include <Scheduler.h>
#include <UserIO.h>

#define DATASTREAM_BUFSIZE_DEFAULT 1024

typedef struct {
    uint8_t* data;
    size_t len;
} stream_packet_t;

class Stream {
protected:
    List<Thread*> waiting;

public:
    virtual void Wait();

    ErrorOr<int64_t> ReadRaw(uint8_t* buffer, size_t len);
    virtual ErrorOr<int64_t> read(UIOBuffer* buffer, size_t len) = 0;
    ErrorOr<int64_t> PeekRaw(uint8_t* buffer, size_t len);
    virtual ErrorOr<int64_t> Peek(UIOBuffer* buffer, size_t len) = 0;
    ErrorOr<int64_t> WriteRaw(const uint8_t* buffer, size_t len);
    virtual ErrorOr<int64_t> write(UIOBuffer* buffer, size_t len) = 0;

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
    ~DataStream() override;

    void Wait() override;

    ErrorOr<int64_t> read(UIOBuffer* buffer, size_t len) override;
    ErrorOr<int64_t> Peek(UIOBuffer* buffer, size_t len) override;
    ErrorOr<int64_t> write(UIOBuffer* buffer, size_t len) override;
    
    int64_t Pos() override { return bufferPos; }
    int64_t Empty() override;
};

class PacketStream final : public Stream {
    List<stream_packet_t> packets;
public:
    void Wait() override;

    ErrorOr<int64_t> read(UIOBuffer* buffer, size_t len) override;
    ErrorOr<int64_t> Peek(UIOBuffer* buffer, size_t len) override;
    ErrorOr<int64_t> write(UIOBuffer* buffer, size_t len) override;
    int64_t Empty() override;
};