# 网络上机报告：TCP协议实验

## 概述

本实验要求我们实现互联网协议栈中传输层中的TCP协议，主要需要完成客户端角色、“停-等”模式的TCP协议，需要能够正确完成客户端TCP的有限状态机，能够正确的与服务器完成三次握手和四次挥手，正确接受和发送TCP报文，然后向应用层提供经典的Berkeley Socket接口。

## 感想

整个TCP协议功能非常强大，利用下一层网络层的IP协议为上层应用程序开发者提供了一个透明的字节流传输，并且还完成了流量控制、拥塞控制等等功能。可以想见，TCP协议的实现也一定不容易。在本次实验中我们仅仅实现了一个最简单的停等式的、无关拥塞控制的TCP协议，其实已经比之前的链路层、网络层协议的代码量高了许多。**设计这样一个复杂、精致的协议确实十分不容易，尝试实现它有助于让我们了解其中的种种细节。**

但是我再实现的过程中还是产生了一些问题，目前暂时还没有找到答案。先记录在此，待之后复习时一并解决：

1. TCP协议拆除连接时发送FIN或者FIN+ACK有什么区别，为什么NetRiver系统中发送FIN就会让整个系统崩溃？
2. `stud_tcp_input()`为什么需要验证`head->ackNo == (tcb->seq + seqAdd)`

## 个人实现

有关TCP协议实现，以及滑动窗口、拥塞窗口、往返时延估计等等具体内容，在指导手册和Tanenbaum的教材上已经做了详细的讲解，因此我在这里不在做赘述，直接进入个人的实现部分。

### TCB表

利用C++STL中的Map实现，维护了一个$port \rightarrow TCB$ 的映射TCBMap。增加、拆除socket连接就是增加或者删除TCBMap中的表项。

这里我发现一个很重要的事情是：由于`TCB`是动态创建的，因此在拆除TCP连接或者说socket接口中需要`close`的时候就需要手动delete TCB。删除TCB一定要准确及时，否则可能导致内存泄漏。

### 底层函数

这一部分主要需要我们实现`stud_tcp_input()`和`stud_tcp_output()`。

#### `stud_tcp_input()`

`stud_tcp_input()`主要需要完成如下的工作：

1. 转换收到TCP段的字节序

2. 检查TCP段的校验和

3. 检查TCP段的ACK是否和本地的SEQ一致、TCP段的SEQ是否和本地的ACK一致

4. 根据TCP段的类型和当前连接的状态进行相应的操作


首先，计算checksum的函数与之前网络层计算不太一样，需要将网络层的目的地址和原地址一并加入计算，并且由于TCP层是不定长度的，因此需要将变长的data部分也加入校验和的计算中去。

进行相应的操作包括：

- 如果当前状态为`SYN_SENT`，且TCP段类型为`PACKET_TYPE_SYN_ACK`表明完成了TCP的握手。需要将状态置为`ESTABLISHED`，然后向服务器发送类型为`PACKET_TYPE_ACK`的确认帧
- 如果当前状态为`ESTABLISHED`且TCP段类型为`PACKET_TYPE_ACK`表明完成了接收到了确认帧，则需要更新TCB的seq和ack号码
- 如果当前状态为`FIN_WAIT1`且TCP段类型为`PACKET_TYPE_ACK`表明完成了本方向的TCP连接拆除，则需要更新TCB的seq和ack号码，将状态置为`FIN_WAIT2`
- 如果当前状态为`FIN_WAIT2`且TCP段的类型为`PACKET_TYPE_FIN_ACK`则对方请求拆除TCP连接，则需要将状态置为`TIME_WAIT`，向对方发送确认帧

#### `stud_tcp_output()`

`stud_tcp_input()`主要需要完成如下的工作：

1. 根据传入的参数构建TCP段
2. 调用`tcp_sendIpPkt()`调用网络层传输TCP包
3. 如果当前状态为`CLOSED` 且发送的段类型为`PACKET_TYPE_SYN`，表明开始建立TCP连接，需要将状态置为`PACKET_TYPE_SYN`
4. 如果当前状态为`ESTABLISHED`且发送的段为`PACKET_TYPE_FIN`，表明开始请求拆除TCP连接，则需要将状态置为`FIN_WAIT1`

### Berkeley Socket

#### `stud_tcp_socket()`

创建新的TCB控制块，插入到TCBMap中，返回当前socket的描述符。

#### `stud_tcp_connect()`

在TCBMap中验证传入的socket描述符是否存在，其状态是否为`CLOSED`。然后调用`stud_tcp_output()`发送TCP段，请求服务器进行TCP连接。并且使用`stud_tcp_input()`验证响应TCP段是否合法。

#### `stud_tcp_send()`

在TCBMap中验证传入的socket描述符是否存在，其状态是否为`ESTABLISHED`。

然后调用`stud_tcp_output()`发送TCP段，向服务器发送相关数据。并且使用`stud_tcp_input()`验证响应TCP段是否合法。

#### `stud_tcp_recv()`

在TCBMap中验证传入的socket描述符是否存在，其状态是否为`ESTABLISHED`。

然后调用`waitIpPacket()`接收IP报文，并且使用`stud_tcp_input()`验证响应TCP段是否合法。

#### `stud_tcp_close()`

在TCBMap中验证传入的socket描述符是否存在，其状态是否为`ESTABLISHED`，如果其状态非`ESTABLISHED`则直接删除TCBMap中的表项，然后退出即可。

然后两次调用`waitIpPacket()`接收IP报文，并且使用`stud_tcp_input()`验证响应TCP段是否合法，完成TCP连接拆除的“四次挥手”。

## 源代码

```c
/*
    Author: David Xiang
    Email: xdw@pku.edu.cn
*/

/*
* THIS FILE IS FOR TCP TEST
*/

#include "sysInclude.h"
#include <map>
using namespace std;

extern void tcp_DiscardPkt(char *pBuffer, int type);
extern void tcp_sendReport(int type);
extern void tcp_sendIpPkt(unsigned char *pData, UINT16 len, unsigned int  srcAddr, unsigned int dstAddr, UINT8  ttl);
extern int waitIpPacket(char *pBuffer, int timeout);
extern unsigned int getIpv4Address();
extern unsigned int getServerIpv4Address();

int gSrcPort = 2005;
int gDstPort = 2006;
int gSeqNum = 1;
int gAckNum = 1;
int gSockNum = 1;

typedef struct TCPHead{
    UINT16 srcPort;
    UINT16 destPort;
    UINT32 seqNo;
    UINT32 ackNo;
    UINT8  headLen;
    UINT8  flag;  
    UINT16 windowsize;
    UINT16 checksum;
    UINT16 urgentPointer;
    char data[100];  
    
    // methods
    void ntoh(){
        srcPort = ntohs(srcPort);
        destPort = ntohs(destPort);
        seqNo = ntohl(seqNo);
        ackNo = ntohl(ackNo);
        windowsize = ntohs(windowsize);
        checksum = ntohs(checksum);
        urgentPointer = ntohs(urgentPointer);
    }

    unsigned int getChecksum(unsigned int srcAddr, unsigned int dstAddr, int type, int len){
        unsigned int sum = 0;
        sum += srcPort + destPort;
        sum += (seqNo >> 16) + (seqNo & 0xffff);
        sum += (ackNo >> 16) + (ackNo & 0xffff);
        sum += (headLen << 8) + flag;
        sum += windowsize + urgentPointer;
        sum += (srcAddr >> 16) + (srcAddr & 0xffff);
        sum += (dstAddr >> 16) + (dstAddr & 0xffff);
        sum += IPPROTO_TCP;
        sum += 0x14;

        if (type == 1){ // also compute data's checksum
		printf("[CHECKSUM] type = 1, len = %d", len);
            sum += len;
            for (int i = 0; i < len; i += 2)
                sum += (data[i] << 8) + (data[i + 1] & 0xFF);
        }
        sum += (sum >> 16);
        return (~sum) & 0xFFFF;
    }
};

enum status{
    CLOSED,
    SYN_SENT,
    ESTABLISHED,
    FIN_WAIT1,
    FIN_WAIT2,
    TIME_WAIT
};

typedef struct TCB{
    unsigned int srcAddr;
    unsigned int dstAddr;
    unsigned short srcPort;
    unsigned short dstPort;
    unsigned int seq;
    unsigned int ack;
    int sockfd;
    BYTE state; 
    unsigned char* data;
    void init(){
        srcAddr = getIpv4Address();
        srcPort = gSrcPort++;
        seq = gSeqNum++;
        ack = gAckNum;
        sockfd = gSockNum++;
        state = CLOSED;
    }
};

TCB *tcb;
map<int, TCB*> TCBMap;

void Log(char* TAG, char* msg){
    printf("[%s]: %s\n", TAG, msg);
}

int stud_tcp_input(char *pBuffer, unsigned short len, unsigned int srcAddr, unsigned int dstAddr){
    char TAG[100] = "stud_tcp_input";
    Log(TAG, "called");
    srcAddr = ntohl(srcAddr);
    dstAddr = ntohl(dstAddr);
    TCPHead* head = (TCPHead *)pBuffer;
    // step 1: convert byte order
    head->ntoh();

    // step 2: check checksum
    if (head->getChecksum(srcAddr, dstAddr, (head->flag==PACKET_TYPE_DATA), len-20) != head->checksum){
        Log(TAG, "Error 1");
        tcp_DiscardPkt()?
        return -1;
    }

    // step 3: check seqNo
    int seqAdd = 1;
    if (tcb->state == FIN_WAIT2)    // ???
        seqAdd = 0;
    else if (len > 20)
        seqAdd = len - 20;
    if (head->ackNo != (tcb->seq + seqAdd)){
        Log(TAG, "Error 2");
        printf("head->ackNo=%d, tcb->seq=%d, seqAdd=%d\n", head->ackNo, tcb->seq, seqAdd);
        tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
        return -1;
    }

    Log(TAG, "handling packet...");
    // step 4: handle data according to status
    if (tcb->state == SYN_SENT && head->flag == PACKET_TYPE_SYN_ACK){
        Log(TAG, "case 1");
        // update seq & ack and send ACK to confirm connection
        Log(TAG, "convert state to ESTABLISHED");
        tcb->state = ESTABLISHED;
        tcb->seq = head->ackNo;
        tcb->ack = head->seqNo + seqAdd;
        stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);  // ??? when to init tcb
        return 0;
    } else if (tcb->state == ESTABLISHED && head->flag == PACKET_TYPE_ACK){
        Log(TAG, "case 2");
        // update seq & ack and do nothing
        tcb->seq = head->ackNo;
        tcb->ack = head->seqNo + seqAdd;
        return 0;
    } else if (tcb->state == FIN_WAIT1 && head->flag == PACKET_TYPE_ACK){
        Log(TAG, "case 3");
        Log(TAG, "convert state to FIN_WAIT2");
        tcb->seq = head->ackNo;
        tcb->ack = head->seqNo + seqAdd;
        tcb->state = FIN_WAIT2;
        return 0;
    } else if (tcb->state == FIN_WAIT2 && head->flag == PACKET_TYPE_FIN_ACK){
        Log(TAG, "case 4");
        Log(TAG, "convert state to TIME_WAIT");
        tcb->state = TIME_WAIT;
        stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
        return 0;
    }
    Log(TAG, "exited");
    return -1;
}

void stud_tcp_output(char *pData, unsigned short len, unsigned char flag, 
    unsigned short srcPort, unsigned short dstPort, unsigned int srcAddr, unsigned int dstAddr){
    char TAG[100] = "stud_tcp_output";
    Log(TAG, "called");
    if (tcb == NULL){
        Log(TAG, "init TCB");
        tcb = new TCB();
        tcb->init();
        tcb->srcPort = srcPort;
        tcb->dstPort = dstPort;
        tcb->srcAddr = srcAddr;
        tcb->dstAddr = dstAddr;
    }
    
    Log(TAG, "constucting TCP packet");
    // constuct a TCP packet
    TCPHead* head = new TCPHead();
    head->srcPort = srcPort;
    head->destPort = dstPort;
    head->seqNo = tcb->seq;
    head->ackNo = tcb->ack;
    head->headLen = 0x50;
    head->flag = flag;
    head->windowsize = 1;
    memcpy(head->data, pData, len);
    head->checksum = head->getChecksum(srcAddr, dstAddr, (flag == PACKET_TYPE_DATA), len);
    head->ntoh();
    tcp_sendIpPkt((unsigned char*)head, len+20, srcAddr, dstAddr, 60);
    delete head;

    if (tcb->state == CLOSED && flag == PACKET_TYPE_SYN){
        // convert state to SYN_SENT
        tcb->state = SYN_SENT;
        Log(TAG, "convert state to SYN_SENT");
    } else if (tcb->state == ESTABLISHED && (flag == PACKET_TYPE_FIN || flag == PACKET_TYPE_FIN_ACK)){
        tcb->state = FIN_WAIT1;
        Log(TAG, "convert state to FIN_WAIT1");
    }
    Log(TAG, "exited");
}

int stud_tcp_socket(int domain, int type, int protocol){
    char TAG[100] = "stud_tcp_socket";
    Log(TAG, "called");
    TCB* tmp = new TCB();
    tmp->init();
    TCBMap.insert(pair<int, TCB*>(tmp->sockfd, tmp));
    Log(TAG, "exited");
    return tmp->sockfd;
}

int stud_tcp_connect(int sockfd, struct sockaddr_in *addr, int addrlen){
    char TAG[100] = "stud_tcp_connect";
    Log(TAG, "called");
    map<int, TCB*>::iterator iterator = TCBMap.find(sockfd);
    if (iterator == TCBMap.end() || iterator->second->state != CLOSED){
        // wrong socket file descriptor
        Log(TAG, "Error 1");
        return -1;
    }
    tcb = iterator->second;
    tcb->dstAddr = htonl(addr->sin_addr.s_addr);
    tcb->dstPort = ntohs(addr->sin_port); // ??? why
    tcb->state = SYN_SENT;
    stud_tcp_output(NULL, 0, PACKET_TYPE_SYN, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
    TCPHead* thead = new TCPHead();
    int res = waitIpPacket((char*)thead, 5000);
    while (res == -1)
        res = waitIpPacket((char*)thead, 5000);
    if (stud_tcp_input((char*)thead, 20, ntohl(tcb->srcAddr), ntohl(tcb->dstAddr)) == 0){
        delete thead;
        Log(TAG, "exited");
        return 0;
    }
    delete thead;
    Log(TAG, "Error 2");
    return -1;
}

int stud_tcp_send(int sockfd, const unsigned char *pData, unsigned short datalen, int flags){
    char TAG[100] = "stud_tcp_send";
    Log(TAG, "called");
    map<int, TCB*>::iterator iterator = TCBMap.find(sockfd);
    if (iterator == TCBMap.end() || iterator->second->state != ESTABLISHED){
        // wrong socket descriptor
        Log(TAG, "Error 1");
        return -1;
    }
    tcb = iterator->second;
    stud_tcp_output((char*)pData, datalen, PACKET_TYPE_DATA, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
    // Wait for response
    TCPHead* thead = new TCPHead();
    int res = waitIpPacket((char*)thead, 5000);
    while (res == -1)
        res = waitIpPacket((char*)thead, 5000);

    if (stud_tcp_input((char*)thead, 20+datalen, ntohl(tcb->srcAddr), ntohl(tcb->dstAddr)) == 0){
        delete thead;
        Log(TAG, "exited");
        return 0;
    }
    delete thead;
    Log(TAG, "Error 2");
    return -1;    
}

int stud_tcp_recv(int sockfd, unsigned char *pData, unsigned short datalen, int flags){
    char TAG[100] = "stud_tcp_recv";
    Log(TAG, "called");
    map<int, TCB*>::iterator iterator = TCBMap.find(sockfd);
    if (iterator == TCBMap.end() || iterator->second->state != ESTABLISHED){
        // wrong socket descriptor
        Log(TAG, "Error 1");
        return -1;
    }
    tcb = iterator->second;

    // Wait for response
    TCPHead* thead = new TCPHead();
    int res = waitIpPacket((char*)thead, 5000);
    while (res == -1)
        res = waitIpPacket((char*)thead, 5000);

    memcpy(pData, thead->data, sizeof(thead->data));
    stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
    delete thead;
    Log(TAG, "exited");
    return 0;
}

int stud_tcp_close(int sockfd){
    char TAG[100] = "stud_tcp_close";
    Log(TAG, "called");
    map<int, TCB*>::iterator iterator = TCBMap.find(sockfd);
    if (iterator == TCBMap.end() || iterator->second->state != ESTABLISHED){
        if (iterator != TCBMap.end()){
            // just delete TCB and return
            TCBMap.erase(iterator);
            Log(TAG, "delete TCB");
            return 0;
        }
        // wrong socket descriptor
        Log(TAG, "Error 1");
        return -1;
    }
    tcb = iterator->second;

    stud_tcp_output(NULL, 0, PACKET_TYPE_FIN, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
    tcb->state = FIN_WAIT1;
    // Wait for response
    TCPHead* thead = new TCPHead();
    int res = waitIpPacket((char*)thead, 5000);
    while (res == -1)
        res = waitIpPacket((char*)thead, 5000);

    if (stud_tcp_input((char*)thead, 20, ntohl(tcb->srcAddr), ntohl(tcb->dstAddr)) == 0){
        delete thead;
        thead = new TCPHead();
        res = waitIpPacket((char*)thead, 5000);
        while (res == -1)
            res = waitIpPacket((char*)thead, 5000);
        if (stud_tcp_input((char*)thead, 20, ntohl(tcb->srcAddr), ntohl(tcb->dstAddr)) == 0){
            TCBMap.erase(iterator);
            delete thead;
            Log(TAG, "exited");
            return 0;
        }
        TCBMap.erase(iterator);
        delete thead;
        Log(TAG, "Error 2");
        return -1;
    }
    TCBMap.erase(iterator);
    delete thead;
    Log(TAG, "Error 3");
    return -1;    
}

```

