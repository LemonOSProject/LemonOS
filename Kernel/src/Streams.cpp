#include <Stream.h>

#include <Assert.h>
#include <Logging.h>
#include <Timer.h>

int64_t Stream::Read(void* buffer, size_t len) {
    assert(!"Stream::Read called from base class");

    return -1; // We should not return but get the compiler to shut up
}

int64_t Stream::Write(void* buffer, size_t len) {
    assert(!"Stream::Write called from base class");

    return -1; // We should not return but get the compiler to shut up
}

int64_t Stream::Peek(void* buffer, size_t len) {
    assert(!"Stream::Peek called from base class");

    return -1; // We should not return but get the compiler to shut up
}

int64_t Stream::Empty() { return 1; }

void Stream::Wait() { assert(!"Stream::Wait called from base class"); }

Stream::~Stream() {}

DataStream::DataStream(size_t bufSize) {
    bufferSize = bufSize;
    bufferPos = 0;

    buffer = reinterpret_cast<uint8_t*>(kmalloc(bufferSize));
}

DataStream::~DataStream() { kfree(buffer); }

int64_t DataStream::Read(void* data, size_t len) {
    acquireLock(&streamLock);

    if (len >= bufferPos)
        len = bufferPos;

    if (!len) {
        releaseLock(&streamLock);
        return 0;
    }

    memcpy(data, buffer, len);

    memcpy(buffer, buffer + len, bufferPos - len);
    bufferPos -= len;

    releaseLock(&streamLock);

    return len;
}

int64_t DataStream::Peek(void* data, size_t len) {
    acquireLock(&streamLock);

    if (len >= bufferPos)
        len = bufferPos;

    if (!len) {
        releaseLock(&streamLock);
        return 0;
    }

    memcpy(data, buffer, len);

    releaseLock(&streamLock);

    return len;
}

int64_t DataStream::Write(void* data, size_t len) {
    acquireLock(&streamLock);
    if (bufferPos + len >= bufferSize) {
        void* oldBuffer = buffer;

        while (bufferPos + len >= bufferSize)
            bufferSize *= 2;

        buffer = reinterpret_cast<uint8_t*>(kmalloc(bufferSize));

        memcpy(buffer, oldBuffer, bufferPos);

        kfree(oldBuffer);
    }

    memcpy(buffer + bufferPos, data, len);
    bufferPos += len;

    if (bufferPos > 0 && waiting.get_length() > 0) {
        while (waiting.get_length()) {
            // Scheduler::UnblockThread(waiting.remove_at(0));
            // TODO: Reinstate blocker
        }
    }

    releaseLock(&streamLock);

    return len;
}

int64_t DataStream::Empty() { return !bufferPos; }

void DataStream::Wait() {
    if (!Empty())
        return;

    // Scheduler::BlockCurrentThread(waiting, streamLock);
    // TODO: Reinstate blocker

    while (Empty()) {
        Scheduler::Yield();
    }
}

int64_t PacketStream::Read(void* buffer, size_t len) {
    if (packets.get_length() <= 0)
        return 0;

    stream_packet_t pkt = packets.remove_at(0);

    if (len > pkt.len)
        len = pkt.len;

    memcpy(buffer, pkt.data, len);

    kfree(pkt.data);

    return len;
}

int64_t PacketStream::Peek(void* buffer, size_t len) {
    if (packets.get_length() <= 0)
        return 0;

    stream_packet_t pkt = packets.get_at(0);

    if (len > pkt.len)
        len = pkt.len;

    memcpy(buffer, pkt.data, len);

    return len;
}

int64_t PacketStream::Write(void* buffer, size_t len) {
    stream_packet_t pkt;
    pkt.len = len;
    pkt.data = reinterpret_cast<uint8_t*>(kmalloc(len));
    memcpy(pkt.data, buffer, len);

    packets.add_back(pkt);

    if (packets.get_length() > 0 && waiting.get_length() > 0) {
        while (waiting.get_length()) {
            // Scheduler::UnblockThread(waiting.remove_at(0));
            // TODO: Reinstate blocker
        }
    }

    return pkt.len;
}

int64_t PacketStream::Empty() { return !packets.get_length(); }

void PacketStream::Wait() {
    while (Empty()) {
        // Scheduler::BlockCurrentThread(waiting, );
    }
}