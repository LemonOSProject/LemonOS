#include <stdio.h> 
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

unsigned int operator-(const timespec& t1, const timespec& t2){
    return (t1.tv_sec - t2.tv_sec) * 1000000000 + (t1.tv_nsec - t2.tv_nsec);
}
timespec udsTimer;

sockaddr_un udsAddr;
void* Thread(void* n){
    int udsClient = socket(AF_UNIX, SOCK_STREAM, 0);
    if(connect(udsClient, reinterpret_cast<sockaddr*>(&udsAddr), sizeof(sockaddr_un))){
        perror("Error connecting to socket:");
        exit(1);
    }

    char buf[513];
    ssize_t len = recv(udsClient, buf, 512, 0);
    buf[len] = 0;

    timespec newTimer;
    clock_gettime(CLOCK_BOOTTIME, &newTimer);

    printf("Recieved message: %s in %d us\n", buf, (newTimer - udsTimer) / 1000);
    
    return nullptr;
}

pthread_t t1;

int udsListener;
int main(){

    udsAddr.sun_family = AF_UNIX;
    strcpy(udsAddr.sun_path, "testsocket");

    udsListener = socket(AF_UNIX, SOCK_STREAM, 0);

    bind(udsListener, reinterpret_cast<sockaddr*>(&udsAddr), sizeof(sockaddr_un));
    listen(udsListener, 0);

    pthread_create(&t1, nullptr, Thread, (void*)"Thread 1\n");

    int fd = accept(udsListener, nullptr, nullptr);

    clock_gettime(CLOCK_BOOTTIME, &udsTimer);

    char msg[] = "Test message";
    send(fd, msg, strlen(msg), 0);

    pthread_join(t1, nullptr);
    return 0;
}