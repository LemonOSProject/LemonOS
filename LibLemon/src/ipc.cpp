#include <lemon/syscall.h>
#include <lemon/ipc.h>

namespace Lemon{
	uint64_t ReceiveMessage(ipc_message_t* msg){
		uint64_t queue_size;
		syscall(SYS_RECEIVE_MESSAGE, (uintptr_t)msg /*Pointer to message*/, (uintptr_t)&queue_size /*Pointer to value with amount of messages in queue*/, 0, 0, 0);
		return queue_size;
	}

	void SendMessage(uint64_t pid, ipc_message_t msg){
		uint64_t m = msg.msg;
		uint64_t data = msg.data;
		uint64_t data2 = msg.data2;

		syscall(SYS_SEND_MESSAGE, (uintptr_t)pid /*Target PID*/, (uintptr_t)m /*Message ID*/, (uintptr_t)data/*Message Data*/, (uintptr_t)data2/*Second Message Data*/, 0);
	}
}