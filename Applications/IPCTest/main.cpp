#include <stdio.h> 
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include <lemon/syscall.h>

unsigned int operator-(const timespec& t1, const timespec& t2){
    return (t1.tv_sec - t2.tv_sec) * 1000000000 + (t1.tv_nsec - t2.tv_nsec);
}
timespec timer;

sockaddr_un udsAddr;
void* UDSThread(void* n){
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

    printf("Recieved message: %s in %d us\n", buf, (newTimer - timer) / 1000);
    
    return nullptr;
}

void* LemonIPCThread(void* n){
    long endpHandle = syscall(SYS_INTERFACE_CONNECT, "lemon.testservice/testif", 0, 0, 0, 0);
    if(endpHandle <= 0){
        printf("Failed to connect to interface %s!", "lemon.testservice/testif");
        return nullptr;
    }
    
    char buf[513];
    uint64_t id;
    uint32_t size;

    long ret = syscall(SYS_ENDPOINT_DEQUEUE, endpHandle, &id, &size, buf, 0);
    while(!ret){
        ret = syscall(SYS_ENDPOINT_DEQUEUE, endpHandle, &id, &size, buf, 0);
    }

    if(ret <= 0){
        printf("Error on endpoint dequeue!\n");
        return nullptr;
    }
    buf[size] = 0;

    timespec newTimer;
    clock_gettime(CLOCK_BOOTTIME, &newTimer);

    printf("Recieved message: %s in %d us\n", buf, (newTimer - timer) / 1000);

    return nullptr;
}

pthread_t t1;
pthread_t t2;

long svcHandle;
long ifHandle;
long endpHandle;

int udsListener;
int main(){

    udsAddr.sun_family = AF_UNIX;
    strcpy(udsAddr.sun_path, "testsocket");

    udsListener = socket(AF_UNIX, SOCK_STREAM, 0);

    if(bind(udsListener, reinterpret_cast<sockaddr*>(&udsAddr), sizeof(sockaddr_un) ) < 0){
        printf("Error binding socket to address!\n");
        return 1;
    }

    listen(udsListener, 0);

    pthread_create(&t1, nullptr, UDSThread, nullptr);

    int fd = accept(udsListener, nullptr, nullptr);

    clock_gettime(CLOCK_BOOTTIME, &timer);

    char msg[] = "Test message";
    send(fd, msg, strlen(msg), 0);

    timespec sendTimer;
    clock_gettime(CLOCK_BOOTTIME, &sendTimer);
    printf("Sending message took %d us\n", (sendTimer - timer));

    pthread_join(t1, nullptr);

    svcHandle = syscall(SYS_CREATE_SERVICE, "lemon.testservice", 0, 0, 0, 0);
    if(svcHandle <= 0){
        printf("Failed to create service!");
        return 1;
    }

    ifHandle = syscall(SYS_CREATE_INTERFACE, svcHandle, "testif", 512, 0, 0);
    if(svcHandle <= 0){
        printf("Failed to create interface!");
        return 1;
    }

    pthread_create(&t2, nullptr, LemonIPCThread, nullptr);

    long endpHandle = syscall(SYS_INTERFACE_ACCEPT, ifHandle, 0, 0, 0, 0);
    while(!endpHandle){
        endpHandle = syscall(SYS_INTERFACE_ACCEPT, ifHandle, 0, 0, 0, 0);
    }

    if(endpHandle <= 0){
        printf("Error accepting connection on interface!");
        return 0;
    }

    clock_gettime(CLOCK_BOOTTIME, &timer);
    syscall(SYS_ENDPOINT_QUEUE, endpHandle, 0xDEADBEEF, strlen(msg), msg, 0);

    clock_gettime(CLOCK_BOOTTIME, &sendTimer);
    printf("Sending message took %d us\n", (sendTimer - timer));

    pthread_join(t2, nullptr);

    return 0;
}