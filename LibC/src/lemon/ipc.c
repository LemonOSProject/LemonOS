#include <core/syscall.h>
#include <core/ipc.h>
/*
uint32_t ReceiveMessage(ipc_message_t* msg){
	uint32_t queue_size;
	syscall(SYS_MSG_RECIEVE, msg /*Pointer to message* /, &queue_size /*Pointer to value with amount of messages in queue* /, 0, 0, 0);
	return queue_size;
}

void SendMessage(uint32_t pid, ipc_message_t msg){
	uint32_t m = msg.id;
	uint32_t data = msg.data;
	uint32_t data2 = msg.data2;

	syscall(SYS_MSG_SEND, pid /*Target PID* /, m /*Message ID* /, data/*Message Data* /, data2/*Second Message Data* /, 0);
}*/