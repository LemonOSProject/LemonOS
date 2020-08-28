#include <stdio.h> 
#include <pthread.h>

void* PrintMessage(void* msg){
    printf((char*)msg);

    return nullptr;
}

pthread_t t1, t2, t3;

int main(){
    pthread_create(&t1, nullptr, PrintMessage, (void*)"Thread 1\n");
    pthread_create(&t2, nullptr, PrintMessage, (void*)"Thread 2\n");
    pthread_create(&t3, nullptr, PrintMessage, (void*)"Thread 3\n");

    printf("Main Thread\n");

    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);
    pthread_join(t3, nullptr);

    return 0;
}