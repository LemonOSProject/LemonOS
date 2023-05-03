#include <UserIO.h>

#include <Error.h>

UIOBuffer UIOBuffer::from_user(void* buffer) {
    return UIOBuffer{buffer, 0, true};
}
