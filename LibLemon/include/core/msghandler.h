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

    class MessageHandler {
    protected:
        friend class MessageMultiplexer;

        virtual std::vector<pollfd>& GetFileDescriptors() = 0;
    };

    class MessageClient : public MessageHandler{
        std::deque<std::shared_ptr<LemonMessage>> queue;
        
        pollfd sock;

        std::vector<pollfd>& GetFileDescriptors();
    public:
        MessageClient();
        ~MessageClient();

        void Connect(sockaddr_un& address, socklen_t len);

        std::shared_ptr<LemonMessage> Poll();
        std::shared_ptr<LemonMessage> PollSync();
        void Wait();
        void Send(LemonMessage* ev);
    };

    class MessageServer{
        std::vector<pollfd> fds;
        std::deque<std::shared_ptr<LemonMessageInfo>> queue;

        int sock = 0;

        std::vector<pollfd>& GetFileDescriptors();
    public:
        MessageServer(sockaddr_un& address, socklen_t len);

        std::shared_ptr<LemonMessageInfo> Poll();
        void Send(LemonMessage* ev, int fd);
    };

    class MessageMultiplexer {
        std::vector<MessageHandler> handlers;
    public:
        void AddSource(MessageHandler& handler);
        bool PollSync();
    };
}