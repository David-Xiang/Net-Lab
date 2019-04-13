/*
	Author: David Xiang
	Email: xdw@pku.edu.cn
*/

#include "sysinclude.h"
#include <queue>
#include <stack>
using namespace std;

typedef enum {
	data, 
	ack,
	nak
} frame_kind;

struct frame_head{
	frame_kind kind;
	unsigned int seq;
	unsigned int ack;
	unsigned char data[100];
};

struct frame{
	frame_head head;
	unsigned int size;
};

deque<frame> waitQueue, windowQueue;

extern void SendFRAMEPacket(unsigned char* pData, unsigned int len);
int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8 messageType);
int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8 messageType);
int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType);
void handle_message(char *pBuffer, int bufferSize, UINT8 messageType);
void handle_timeout(char* pBuffer);
void handle_send(char* pBuffer, int bufferSize);
int handle_receive(char* pBuffer, int bufferSize);
void send_from_wait_queue();

#define WINDOW_SIZE_STOP_WAIT 1
#define WINDOW_SIZE_BACK_N_FRAME 4

int senderWindowSize = -1;	// global variable for identifying which protocol is now in use

int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8 messageType){
	// initialize parameters if not initialized
	if (senderWindowSize = -1)
		senderWindowSize = WINDOW_SIZE_STOP_WAIT;

	handle_message(*pBuffer, bufferSize, messageType);
	return 0;
}

int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8 messageType){
	// initialize parameters if not initialized
	if (senderWindowSize = -1)
		senderWindowSize = WINDOW_SIZE_BACK_N_FRAME;

	handle_message(*pBuffer, bufferSize, messageType);
	return 0;
}

int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType){
	// initialize parameters if not initialized
	if (senderWindowSize = -1)
		senderWindowSize = WINDOW_SIZE_BACK_N_FRAME;

	handle_message(*pBuffer, bufferSize, messageType);
	return 0;
}

void handle_message(char *pBuffer, int bufferSize, UINT8 messageType){
	switch (messageType) {
		case MSG_TYPE_TIMEOUT:
			handle_timeout(pBuffer);
			break;
		case MSG_TYPE_SEND:
			handle_send(pBuffer, bufferSize);
			break;
		case MSG_TYPE_RECEIVE:
			handle_receive(pBuffer, bufferSize);
			break;
		default:
			return -1;
	}
}

void handle_timeout(char* pBuffer){
	// resend all frames in windowQueue
	stack<frame> stk;
	while(!windowQueue.empty()){
		frame f = windowQueue.front();
		windowQueue.pop_front();
		stk.push(f);
	}
	while(!stk.empty()){
		frame f = stk.top();
		stk.pop();
		waitQueue.push_front(f);
	}

	send_from_wait_queue();
}

void handle_send(char* pBuffer, int bufferSize){
	// add this frame to wait queue
	frame f;
	memcpy(&f, pBuffer, sizeof(frame));
	f.size = bufferSize;
	
	// don't fill seq field until send
	waitQueue.push_back(f);

	send_from_wait_queue();
}

void send_from_wait_queue(){
	// send frames in waitQueue until window is maximized
	while(windowQueue.size() < senderWindowSize && waitQueue.size() > 0){
		frame f = waitQueue.front();
		waitQueue.pop_front();
		windowQueue.push_back(f);
		SendFRAMEPacket((unsigned char*)(&f), f.size);
	}
}

int handle_receive(char* pBuffer, int bufferSize){
	// first implement this function in stop-and-wait sense
	
	// no frame was sent
	if (windowQueue.size() == 0)
		return -1;
	
	frame* fp = (frame*) pBuffer;
	if (ntohl(fp->head.kind) == ack){
		// this frame was received
		unsigned int ack = ntohl(fp->head.ack);
		// pop up all frames before the confirmed frame
		while (ntohl(windowQueue.front().head.seq) != ack)
			windowQueue.pop_front();
		windowQueue.pop_front();
	} else if (ntohl(fp->head.kind) == nak){
		// this frame need to be resent
		unsigned int nakindex = ntohl(fp->head.ack);
		for (deque<frame>::iterator it = windowQueue.begin(); it != windowQueue.end(); it++){
			unsigned int waitSeq = ntohl((*it).head.seq);
			if (waitSeq == nakindex){
				SendFRAMEPacket((unsigned char *)(&(*it)), it->size);
				break;
			}
		}
	}
	
	// send other frames
	send_from_wait_queue();
	return 0;
}