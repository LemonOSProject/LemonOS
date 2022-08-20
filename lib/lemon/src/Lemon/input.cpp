#include <Lemon/Core/Input.h>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

namespace Lemon{
    static int mouseFd = 0;
    static int keyboardFd = 0;

    int PollMouse(MousePacket& pkt){
        if(!mouseFd) mouseFd = open("/dev/mouse0", O_RDONLY);

        memset(&pkt, 0, sizeof(MousePacket));

        return read(mouseFd, &pkt, sizeof(MousePacket));
    }

    ssize_t PollKeyboard(uint8_t* buffer, size_t count){
        if(!keyboardFd) keyboardFd = open("/dev/keyboard0", O_RDONLY);

        return read(keyboardFd, buffer, count);
    }
}
