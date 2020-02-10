#ifndef IPC_H
#define IPC_H

#define WINDOW_EVENT_KEY 1
#define WINDOW_EVENT_MOUSEDOWN 2
#define WINDOW_EVENT_MOUSEUP 3
#define WINDOW_EVENT_KEYRELEASED 4
#define WINDOW_EVENT_CLOSE 5

#define DESKTOP_EVENT 0xBEEF
	#define DESKTOP_SUBEVENT_WINDOW_DESTROYED 1
	#define DESKTOP_SUBEVENT_WINDOW_CREATED 2
#define DESKTOP_EVENT_KEY 0x1BEEF
#define DESKTOP_EVENT_KEY_RELEASED 0x2BEEF

typedef struct {
	uint64_t senderPID; // PID of Sender
	uint64_t recieverPID; // PID of Reciever
	uint64_t msg; // ID of message
	uint64_t data; // Message Data
	uint64_t data2;
} __attribute__((packed)) ipc_message_t;

#ifdef __cplusplus
extern "C"{
#endif

void SendMessage(uint64_t pid, ipc_message_t msg);
uint64_t ReceiveMessage(ipc_message_t* msg);

#ifdef __cplusplus
}
#endif

#endif 
