# 网络上机报告：滑动窗口协议

## 概述

本实验要求我们实现一个数据链路层的滑动窗口协议。主要是在一个数据链路层的模拟环境下，用C++实现3个数据链路层协议：

1. 1比特滑动窗口协议
2. 回退N帧滑动窗口协议
3. 选择性重传协议

而我们只需要实现这三个协议的发送方部分，通过与服务器的数据链路层对等体正确的通信来验证协议的正确实现。

## 个人实现

有关滑动窗口的理论解释，在教材和实验指导手册的上已经详细阐述了，因此我在此处并不再做过多阐述。主要谈一谈个人的实现方式。

首先指导手册已经给出了本层协议的具体接口：

```c
int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8 messageType);
int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8 messageType);
int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType);
```

3个函数对应3个链路层的不同协议，但是经过仔细研究可以发现其实3个协议并不是相互独立的，每个协议是向前兼容的：回退N帧滑动窗口协议中的N取1就退化为1比特滑动窗口协议，选择性重传协议则是在回退N帧滑动窗口协议上增加了对NAK确认帧的处理。因此我们只要实现了选择性重传协议其实就实现了3个协议。

### 初始化

 根据调用的接口函数来初始化窗口大小，`stud_slide_window_stop_and_wait`初始化为WINDOW_SIZE_STOP_WAIT，而`stud_slide_window_back_n_frame`,`stud_slide_window_choice_frame_resend`初始化为WINDOW_SIZE_BACK_N_FRAME。然后程序根据事件的不同来分发给不同的handler进行处理

### handle_send

不能够直接发送帧，而需要首先将其加入到缓冲队列waitQueue中。然后调用`send_from_wait_queue`进行发送。

`send_from_wait_queue`函数会在同时满足发送窗口未占满，缓冲队列还有未发送的帧两个条件时，持续从缓冲队列中取出帧进行发送。

### handle_receive

接收到确认帧的话，首先需要判断确认帧的类型。如果是ACK，则将确认序号及之前的所有窗口关闭，即将windowQueue队列中的相应帧出队列（*这里的细节是接收方的协议为累计确认*）。

如果确认帧类型是NAK（选择性重传协议），则表示对应序号帧需要重传，进行重传即可。

在处理完消息之后，可能发送窗口不满，则此时需要调用`send_from_wait_queue`函数继续处理未发送的帧。

**最重要是的在frame数据结构中的序号均是大端法，因此需要使用ntohl函数进行转换。**

### handle_timeout

因为发送方协议采用累计确认，因此timeout的一定是windowQueue队首的帧，其之后的队列中的所有帧均需要重新发送。因此将所有帧全部出队，按照顺序加入waitQueue中重新处理即可。

## 源代码

```c
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
```

