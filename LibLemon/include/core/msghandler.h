#pragma once

#include <core/message.h>
#include <string.h>

namespace Lemon{
    struct LemonMessageInfo {
        int clientFd;
        LemonMessage msg;
    };

    class MessageHandler {
    public:
        virtual std::vector<pollfd> GetFileDescriptors() = 0;
    protected:
        friend class MessageMultiplexer;

        virtual ~MessageHandler() = default;
    };

    class MessageClient : public MessageHandler{
        std::deque<std::shared_ptr<LemonMessage>> queue;
        
        pollfd sock;

        std::vector<pollfd> GetFileDescriptors();
    public:
        MessageClient();
        ~MessageClient();

        void Connect(sockaddr_un& address, socklen_t len);

        std::shared_ptr<LemonMessage> Poll();
        std::shared_ptr<LemonMessage> PollSync();
        void Wait();
        void Send(LemonMessage* msg);
        void Send(const Message& msg);
    };

    class MessageServer : public MessageHandler{
        std::vector<pollfd> fds;
        std::deque<std::shared_ptr<LemonMessageInfo>> queue;

        pollfd sock;

        std::vector<pollfd> GetFileDescriptors();
    public:
        MessageServer(sockaddr_un& address, socklen_t len);

        std::shared_ptr<LemonMessageInfo> Poll();
        void Send(LemonMessage* msg, int fd);
        void Send(const Message& msg, int fd);
    };

    class MessageMultiplexer {
        std::vector<MessageHandler*> handlers;
    public:
        void AddSource(MessageHandler& handler);
        bool PollSync();
    };

    class MessageBusInterface : public MessageClient{
    private:
        static constexpr const char* messageBusSocketAddress = "lemond";
        static MessageBusInterface* instance;

    public:
        MessageBusInterface(){
            sockaddr_un sAddress;
            strcpy(sAddress.sun_path, messageBusSocketAddress);
            sAddress.sun_family = AF_UNIX;
            Connect(sAddress, sizeof(sockaddr_un));
        }

        static MessageBusInterface& Instance(){
            if(!instance){
                instance = new MessageBusInterface();
            } 

            return *instance;
        }

        virtual ~MessageBusInterface() = default;
    };
}