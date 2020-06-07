#include <core/input.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

namespace Lemon{
    int PollMouse(MousePacket& pkt){
        static int mouseFd = 0;

        if(!mouseFd) mouseFd = open("/dev/mouse0", O_RDONLY);

        memset(&pkt, 0, sizeof(MousePacket));

        return read(mouseFd, &pkt, sizeof(MousePacket));
    }

    ssize_t PollKeyboard(uint8_t* buffer, size_t count){
        static int keyboardFd = 0;

        if(!keyboardFd) keyboardFd = open("/dev/keyboard0", O_RDONLY);

        return read(keyboardFd, buffer, count);
    }
}