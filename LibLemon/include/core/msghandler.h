#pragma once

#include <core/message.h>

#include <vector>
#include <deque>
#include <memory>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace Lemon{
    struct LemonMessageInfo {
        int clientFd;
        LemonMessage msg;
    };

    class MessageClient{
        std::deque<std::shared_ptr<LemonMessage>> queue;
        
        pollfd sock;
    public:
        MessageClient();

        void Connect(sockaddr_un& address, socklen_t len);

        std::shared_ptr<LemonMessage> Poll();
        void Send(LemonMessage* ev);
    };

    class MessageServer{
        std::vector<pollfd> fds;
        std::deque<std::shared_ptr<LemonMessageInfo>> queue;

        int sock = 0;
    public:
        MessageServer(sockaddr_un& address, socklen_t len);

        std::shared_ptr<LemonMessageInfo> Poll();
        void Send(LemonMessage* ev, int fd);
    };
}