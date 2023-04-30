#pragma once

#include <Compiler.h>

// Lines up with poll/epoll constants
enum class KOEvent : int {
    None = 0,
    In = 1, // Data is available for reading (reading will not block)
    ProcessTerminated = In, // A process object is no longer running
    Out = 2, // Data is avaialble for writing (writing will not block)
    // Peer closed connection (peer will not recieve or send any more data)
    // There may still be data left to read in buffers
    Hangup = 4,
    // Peer has shutdown write half of connection meaning they will
    // not send any more data (they may still recieve data)
    ReadHangup = 8,
    // Some sort of error has happened for the file
    Error = 0x10,
};

#define HAS_KOEVENT(events, event) (((int)events) & ((int)event))
#define KOEVENT_MASK(events, mask) ((KOEvent)HAS_KOEVENT(events, mask))

ALWAYS_INLINE KOEvent operator|(const KOEvent& l, const KOEvent& r) {
    return (KOEvent)((int)l | (int)r);
}
