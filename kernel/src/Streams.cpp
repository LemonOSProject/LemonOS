#include <Stream.h>

#include <Assert.h>
#include <Logging.h>
#include <Timer.h>

ErrorOr<int64_t> Stream::ReadRaw(uint8_t* buffer, size_t len) {
    UIOBuffer uio = UIOBuffer(buffer);
    return read(&uio, len);
}

ErrorOr<int64_t> Stream::WriteRaw(const uint8_t* buffer, size_t len) {
    UIOBuffer uio = UIOBuffer((uint8_t*)buffer);
    return write(&uio, len);
}

ErrorOr<int64_t> Stream::PeekRaw(uint8_t* buffer, size_t len) {
    UIOBuffer uio = UIOBuffer(buffer);
    return Peek(&uio, len);
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

ErrorOr<int64_t> DataStream::read(UIOBuffer* data, size_t len) {
    acquireLock(&streamLock);

    if (len >= bufferPos)
        len = bufferPos;

    if (!len) {
        releaseLock(&streamLock);
        return 0;
    }

    if(data->write(buffer, len)) {
        return EFAULT;
    }

    memcpy(buffer, buffer + len, bufferPos - len);
    bufferPos -= len;

    releaseLock(&streamLock);

    return len;
}

ErrorOr<int64_t> DataStream::Peek(UIOBuffer* data, size_t len) {
    acquireLock(&streamLock);

    if (len >= bufferPos)
        len = bufferPos;

    if (!len) {
        releaseLock(&streamLock);
        return 0;
    }

    if(data->write(buffer, len)) {
        return EFAULT;
    }

    releaseLock(&streamLock);

    return len;
}

ErrorOr<int64_t> DataStream::write(UIOBuffer* data, size_t len) {
    acquireLock(&streamLock);
    if (bufferPos + len >= bufferSize) {
        void* oldBuffer = buffer;

        while (bufferPos + len >= bufferSize)
            bufferSize *= 2;

        buffer = reinterpret_cast<uint8_t*>(kmalloc(bufferSize));

        memcpy(buffer, oldBuffer, bufferPos);

        kfree(oldBuffer);
    }

    if(data->read(buffer + bufferPos, len)) {
        return EFAULT;
    }

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

ErrorOr<int64_t> PacketStream::read(UIOBuffer* buffer, size_t len) {
    if (packets.get_length() <= 0)
        return 0;

    stream_packet_t pkt = packets.remove_at(0);

    if (len > pkt.len)
        len = pkt.len;

    if(buffer->write(pkt.data, len)) {
        return EFAULT;
    }

    kfree(pkt.data);

    return len;
}

ErrorOr<int64_t> PacketStream::Peek(UIOBuffer* buffer, size_t len) {
    if (packets.get_length() <= 0)
        return 0;

    stream_packet_t pkt = packets.get_at(0);

    if (len > pkt.len)
        len = pkt.len;

    if(buffer->write(pkt.data, len)) {
        return EFAULT;
    }

    return len;
}

ErrorOr<int64_t> PacketStream::write(UIOBuffer* buffer, size_t len) {
    stream_packet_t pkt;
    pkt.len = len;
    pkt.data = reinterpret_cast<uint8_t*>(kmalloc(len));
    if(buffer->read(pkt.data, len)) {
        kfree(pkt.data);
        return EFAULT;
    }

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