#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <list.h>

#include "defines.h"

char* socketAddress = SOCKET_TEST_ADDR;
int socketAddressLength = 0;

List<int> fds;

int main(int argc, char** argv){
    char* data;

    if(argc > 1){
        data = argv[1];
    } else {
        data = "Socket test data";
    }

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if(sock < 0){
        perror("Error Creating Socket");
        return 1;
    }

    socketAddressLength = strlen(socketAddress);

    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketAddress);

    if(bind(sock, (sockaddr*)&addr, sizeof(addr))){
        perror("Error Binding Socket");
        return 2;
    }

    if(listen(sock, 10) < 0){
        perror("Listen Error");
        return 4;
    }

    char buf[101];
    while(1){
        int client = accept(sock, 0, 0);
        if(client < 0){
            perror("Error accepting connection");
            continue;
        }

        int count;
        if(count = read(client, buf, 100)){
            printf("reading\n");
            buf[count] = 0;
            printf("Read %x bytes: %s\n", count, buf); 
        }
        
        if(!count)
            close(client);
    }

    close(sock);

    return 0;
}