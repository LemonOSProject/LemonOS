#ifndef IPC_H
#define IPC_H

#define WINDOW_EVENT_KEY 1
#define WINDOW_EVENT_MOUSEDOWN 2
#define WINDOW_EVENT_MOUSEUP 3

#define DESKTOP_EVENT 0xBEEF
#define DESKTOP_EVENT_KEY 0x1BEEF

typedef struct {
	uint32_t senderPID; // PID of Sender
	uint32_t recieverPID; // PID of Reciever
	uint32_t id; // ID of message
	uint32_t data; // Message Data
} __attribute__((packed)) ipc_message_t;

#ifdef __cplusplus
extern "C"{
#endif

void SendMessage(uint32_t pid, ipc_message_t msg);
uint32_t ReceiveMessage(ipc_message_t* msg);

#ifdef __cplusplus
}
#endif

#endif 
