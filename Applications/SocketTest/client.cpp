#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "defines.h"

char* socketAddress = SOCKET_TEST_ADDR;
int socketAddressLength = 0;

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

    if(connect(sock, (sockaddr*)&addr, sizeof(addr))){
        perror("Error Connecting To Socket");
        return 2;
    }

    write(sock, data, strlen(data));

    close(sock);

    return 0;
}