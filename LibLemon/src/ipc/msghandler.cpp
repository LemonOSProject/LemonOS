#include <core/message.h>
#include <core/msghandler.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

namespace Lemon {
    MessageClient::MessageClient(){
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        assert(fd > 0);

        sock.fd = fd;
        sock.events = POLLIN | POLLHUP;
    }

    MessageClient::~MessageClient(){
        close(sock.fd);
    }

    MessageServer::MessageServer(sockaddr_un& address, socklen_t len){
        sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        assert(sock > 0);

        int e = bind(sock, (sockaddr*)&address, len);
        
        if(e){
            perror("Bind: ");
            close(sock);
            assert(!e);
        }

        e = listen(sock, 16);

        if(e){
            perror("Listen: ");
            close(sock);
            assert(!e);
        }
    }

    void MessageClient::Connect(sockaddr_un& address, socklen_t len){
        printf("connecting\r\n");
        int e = connect(sock.fd, (sockaddr*)&address, len);
        printf("connected\r\n");
        
        if(e){
            close(sock.fd);
            perror("Connect: ");
            assert(!e);
        }
    }

    std::shared_ptr<LemonMessageInfo> MessageServer::Poll(){
    retry:
        if(queue.size() > 0){
            auto element = queue.front();
            queue.pop_front();
            return element;
        }

        int fd = 0;
        while((fd = accept(sock, nullptr, nullptr)) > 0){
            fds.push_back({ .fd = fd, .events = POLLIN, .revents = 0 });
        }

        if(!fds.size()) {
            return std::shared_ptr<LemonMessageInfo>(nullptr);
        }

        int evCount = poll(fds.data(), fds.size(), 0);

        if(evCount > 0){
            for(size_t i = 0; i < fds.size(); i++){
                if(fds[i].revents & (POLLNVAL | POLLHUP)){
                    int fd = fds[i].fd;
                    fds.erase(fds.begin() + i);
                    
                    std::shared_ptr<LemonMessageInfo> newMsg = std::shared_ptr<LemonMessageInfo>((LemonMessageInfo*)malloc(sizeof(LemonMessageInfo)));
                    newMsg->msg.protocol = 0; // Disconnected
                    newMsg->clientFd = fd;

                    return newMsg;
                }

                if(!(fds[i].revents & POLLIN)) continue; // We only care about POLLIN
                LemonMessage msg;

                ssize_t len = recv(fds[i].fd, &msg, sizeof(LemonMessage), 0);
                if(len < static_cast<ssize_t>(sizeof(LemonMessage))){
                    printf("invalid length: %ld\n", len);
                    continue;
                }

                if(msg.magic != LEMON_MESSAGE_MAGIC){
                    printf("Invalid magic: %x, discarding data.\n", msg.magic);
                    while(msg.magic != LEMON_MESSAGE_MAGIC && len >= static_cast<ssize_t>(sizeof(LemonMessage))){
                        len = recv(fds[i].fd, &msg, sizeof(msg), MSG_DONTWAIT); // Discard everything until we find a message
                    }
                }

                if(msg.magic != LEMON_MESSAGE_MAGIC){
                    printf("invalid magic: %x\n", msg.magic);
                    continue;
                }

                std::shared_ptr<LemonMessageInfo> newMsg((LemonMessageInfo*)malloc(sizeof(LemonMessageInfo) + msg.length));
                newMsg->msg = msg;
                newMsg->clientFd = fds[i].fd;

                len = recv(fds[i].fd, newMsg->msg.data, msg.length, 0);

                if(len < msg.length){
                    printf("Warning: invalid message length %u. Only read %ld bytes\n", msg.length, len);
                    continue;
                }

                queue.push_back(newMsg);
            }

            // Check for new messages
            if(queue.size() > 0)
                goto retry;
        }

        return std::shared_ptr<LemonMessageInfo>(nullptr);
    }

    std::shared_ptr<LemonMessage> MessageClient::Poll(){
    retry:
        if(queue.size() > 0){
            auto element = queue.front();
            queue.pop_front();
            return element;
        }

        int ret = poll(&sock, 1, 0);

        if(ret < 0){
            perror("Poll: ");

            return std::shared_ptr<LemonMessage>(nullptr);
        }

        if(!(sock.revents & POLLIN)) return std::shared_ptr<LemonMessage>(nullptr);

        LemonMessage msg;

        ssize_t len = recv(sock.fd, &msg, sizeof(LemonMessage), MSG_DONTWAIT);
        
        if(len < (ssize_t)sizeof(LemonMessage)){
            //printf("invalid length: %d\n", len);
            
            return std::shared_ptr<LemonMessage>(nullptr);
        }

        if(msg.magic != LEMON_MESSAGE_MAGIC){
            printf("Invalid magic: %x, discarding data.\n", msg.magic);
            while(msg.magic != LEMON_MESSAGE_MAGIC && len >= static_cast<ssize_t>(sizeof(msg))){
                len = recv(sock.fd, &msg, sizeof(msg), MSG_DONTWAIT); // Discard everything until we find a message
            }
        }

        if(msg.magic != LEMON_MESSAGE_MAGIC){
            printf("invalid magic: %x\n", msg.magic);
            
            return std::shared_ptr<LemonMessage>(nullptr);
        }

        std::shared_ptr<LemonMessage> newMsg((LemonMessage*)malloc(sizeof(LemonMessage) + msg.length));
        *newMsg = msg;

        len = recv(sock.fd, newMsg->data, msg.length, 0);

        if(len < msg.length){
            printf("Warning: invalid message length %u. Only read %ld bytes\n", msg.length, len);
            
            return std::shared_ptr<LemonMessage>(nullptr);
        }

        queue.push_back(newMsg);

        goto retry;
    }

    std::shared_ptr<LemonMessage> MessageClient::PollSync(){
    retry:
        if(queue.size() > 0){
            auto element = queue.front();
            queue.pop_front();
            return element;
        }

        LemonMessage msg;

        ssize_t len = recv(sock.fd, &msg, sizeof(LemonMessage), 0);
        
        if(len < (ssize_t)sizeof(LemonMessage)){
            //printf("invalid length: %d\n", len);
            
            return std::shared_ptr<LemonMessage>(nullptr);
        }

        if(msg.magic != LEMON_MESSAGE_MAGIC){
            printf("Invalid magic: %x, discarding data.\n", msg.magic);
            while(msg.magic != LEMON_MESSAGE_MAGIC && len >= static_cast<ssize_t>(sizeof(msg))){
                len = recv(sock.fd, &msg, sizeof(msg), MSG_DONTWAIT); // Discard everything until we find a message
            }
        }

        if(msg.magic != LEMON_MESSAGE_MAGIC){
            printf("invalid magic: %x\n", msg.magic);
            
            return std::shared_ptr<LemonMessage>(nullptr);
        }

        std::shared_ptr<LemonMessage> newMsg((LemonMessage*)malloc(sizeof(LemonMessage) + msg.length));
        *newMsg = msg;

        len = recv(sock.fd, newMsg->data, msg.length, 0);

        if(len < msg.length){
            printf("Warning: invalid message length %u. Only read %ld bytes\n", msg.length, len);
            
            return std::shared_ptr<LemonMessage>(nullptr);
        }

        queue.push_back(newMsg);

        goto retry;
    }

    void MessageClient::Wait(){
        char c;
        recv(sock.fd, &c, 0, MSG_PEEK);
    }

    void MessageServer::Send(LemonMessage* msg, int fd){
        if(fd < 0) {
            printf("Invalid fd: %i\n", fd);
            return;
        }

        msg->magic = LEMON_MESSAGE_MAGIC;

        ssize_t sent = send(fd, msg, msg->length + sizeof(LemonMessage), MSG_DONTWAIT);

        if(sent <= 0){
            perror("Warning: Send: ");
        } else if(sent < msg->length + static_cast<short>(sizeof(LemonMessage))){
            printf("Warning: Tried to send %lu bytes, but only sent %ld bytes", msg->length + sizeof(LemonMessage), sent);
        }
    }

    void MessageServer::Send(const Message& msg, int fd){
        if(fd < 0) {
            printf("Invalid fd: %i\n", fd);
            return;
        }

        ssize_t sent = send(fd, msg.data(), msg.length(), MSG_DONTWAIT);

        if(sent < 0){
            perror("Warning: Send: ");
        } else if(sent < msg.length()){
            printf("Warning: Tried to send %u bytes, but only sent %ld bytes", msg.length(), sent);
        }
    }

    void MessageClient::Send(LemonMessage* msg){
        msg->magic = LEMON_MESSAGE_MAGIC;

        ssize_t sent = send(sock.fd, msg, msg->length + sizeof(LemonMessage), 0);

        if(sent <= 0){
            perror("Warning: Send: ");
        } else if(sent < msg->length + static_cast<short>(sizeof(LemonMessage))){
            printf("Warning: Tried to send %lu bytes, but only sent %ld bytes", msg->length + sizeof(LemonMessage), sent);
        }
    }

    void MessageClient::Send(const Message& msg){
        ssize_t sent = send(sock.fd, msg.data(), msg.length(), 0);

        if(sent <= 0){
            perror("Warning: Send: ");
        } else if(sent < msg.length()){
            printf("Warning: Tried to send %u bytes, but only sent %ld bytes", msg.length(), sent);
        }
    }

    std::vector<pollfd> MessageServer::GetFileDescriptors(){
        return fds;
    }

    std::vector<pollfd> MessageClient::GetFileDescriptors(){
        std::vector<pollfd> v;
        v.push_back(sock);
        return v;
    }

    bool MessageMultiplexer::PollSync(){
        std::vector<pollfd> fds;

        for(MessageHandler& h : handlers){
            for(pollfd& f : h.GetFileDescriptors()){
                fds.push_back(f);
            }
        }

        int evCount = poll(fds.data(), fds.size(), -1);

        if(evCount > 0){
            return true;
        }

        return false;
    }

    MessageBusInterface* MessageBusInterface::instance = nullptr;
}