#include <UserIO.h>

#include <Error.h>

UIOBuffer UIOBuffer::FromUser(void* buffer) {
    return UIOBuffer{buffer, 0, true};
}
