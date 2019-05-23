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
    
    void ntoh(){
        srcPort = ntohs(srcPort);
        destPort = ntohs(destPort);
        seqNo = ntohl(seqNo);
        ackNo = ntohl(ackNo);
        windowsize = ntohs(windowsize);
        checksum = ntohs(checksum);
        urgentPointer = ntohs(urgentPointer);
    }

    unsigned int getChecksum(unsigned int srcAddr, unsigned int dstAddr, int containData, int datalen){
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

        if (containData == 1){ // this packet contains data
            sum += datalen;
            for (int i = 0; i < datalen; i += 2)
                sum += (data[i] << 8) + (data[i + 1] & 0xFF);
        }
        sum += (sum >> 16);
        return (~sum) & 0xffff;
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
    // util function for debug
    printf("[%s]: %s\n", TAG, msg);
}

int stud_tcp_input(char *pBuffer, unsigned short len, unsigned int srcAddr, unsigned int dstAddr){
    char TAG[100] = "stud_tcp_input";

    TCPHead* head = (TCPHead *)pBuffer;

    // step 1: convert byte order
    head->ntoh();
    srcAddr = ntohl(srcAddr);
    dstAddr = ntohl(dstAddr);

    // step 2: check checksum
    if (head->getChecksum(srcAddr, dstAddr, (head->flag==PACKET_TYPE_DATA), len-20) != head->checksum){
        Log(TAG, "Error 1");
        return -1;
    }

    // step 3: check seqNo
    int seqAdd = 1;
    if (tcb->state == FIN_WAIT2)
        seqAdd = 0;
    else if (len > 20)  // this packet contains data
        seqAdd = len - 20;
    if (head->ackNo != (tcb->seq + seqAdd)){
        // ？？？
        Log(TAG, "Error 2");
        tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
        return -1;
    }

    // step 4: handle data according to status
    if (tcb->state == SYN_SENT && head->flag == PACKET_TYPE_SYN_ACK){
        // update seq & ack and send ACK to confirm connection
        Log(TAG, "convert state to ESTABLISHED");
        tcb->state = ESTABLISHED;
        tcb->seq = head->ackNo;
        tcb->ack = head->seqNo + seqAdd;
        stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);  // ??? when to init tcb
        return 0;
    } else if (tcb->state == ESTABLISHED && head->flag == PACKET_TYPE_ACK){
        // ？？？
        // update seq & ack and do nothing
        tcb->seq = head->ackNo;
        tcb->ack = head->seqNo + seqAdd;
        return 0;
    } else if (tcb->state == FIN_WAIT1 && head->flag == PACKET_TYPE_ACK){
        Log(TAG, "convert state to FIN_WAIT2");
        tcb->seq = head->ackNo;
        tcb->ack = head->seqNo + seqAdd;
        tcb->state = FIN_WAIT2;
        return 0;
    } else if (tcb->state == FIN_WAIT2 && head->flag == PACKET_TYPE_FIN_ACK){
        Log(TAG, "convert state to TIME_WAIT");
        tcb->state = da;
        stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
        return 0;
    }
    return -1;
}

void stud_tcp_output(char *pData, unsigned short len, unsigned char flag, 
    unsigned short srcPort, unsigned short dstPort, unsigned int srcAddr, unsigned int dstAddr){
    char TAG[100] = "stud_tcp_output";

    if (tcb == NULL){
        Log(TAG, "init TCB");
        tcb = new TCB();
        tcb->init();
        tcb->srcPort = srcPort;
        tcb->dstPort = dstPort;
        tcb->srcAddr = srcAddr;
        tcb->dstAddr = dstAddr;
    }
    
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
        // convert state to FIN_WAIT1
        tcb->state = FIN_WAIT1;
        Log(TAG, "convert state to FIN_WAIT1");
    }
}

int stud_tcp_socket(int domain, int type, int protocol){
    char TAG[100] = "stud_tcp_socket";

    TCB* tmp = new TCB();
    tmp->init();
    TCBMap.insert(pair<int, TCB*>(tmp->sockfd, tmp));
    return tmp->sockfd;
}

int stud_tcp_connect(int sockfd, struct sockaddr_in *addr, int addrlen){
    char TAG[100] = "stud_tcp_connect";

    map<int, TCB*>::iterator iterator = TCBMap.find(sockfd);
    if (iterator == TCBMap.end() || iterator->second->state != CLOSED){
        // wrong socket file descriptor
        Log(TAG, "Error 1");
        return -1;
    }

    tcb = iterator->second;
    tcb->dstAddr = htonl(addr->sin_addr.s_addr);
    tcb->dstPort = ntohs(addr->sin_port);
    stud_tcp_output(NULL, 0, PACKET_TYPE_SYN, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
    TCPHead* thead = new TCPHead();
    int res = waitIpPacket((char*)thead, 5000);
    while (res == -1)
        res = waitIpPacket((char*)thead, 5000);
    
    if (stud_tcp_input((char*)thead, 20, ntohl(tcb->srcAddr), ntohl(tcb->dstAddr)) == 0){
        delete thead;
        return 0;
    }
    // stud_tcp_input returns error
    delete thead;
    Log(TAG, "Error 2");
    return -1;
}

int stud_tcp_send(int sockfd, const unsigned char *pData, unsigned short datalen, int flags){
    char TAG[100] = "stud_tcp_send";

    map<int, TCB*>::iterator iterator = TCBMap.find(sockfd);
    if (iterator == TCBMap.end() || iterator->second->state != ESTABLISHED){
        // wrong socket descriptor
        Log(TAG, "Error 1");
        return -1;
    }

    tcb = iterator->second;
    stud_tcp_output((char*)pData, datalen, PACKET_TYPE_DATA, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
    
    // wait for response
    TCPHead* thead = new TCPHead();
    int res = waitIpPacket((char*)thead, 5000);
    while (res == -1)
        res = waitIpPacket((char*)thead, 5000);

    if (stud_tcp_input((char*)thead, 20+datalen, ntohl(tcb->srcAddr), ntohl(tcb->dstAddr)) == 0){
        delete thead;
        return 0;
    }
    delete thead;
    Log(TAG, "Error 2");
    return -1;    
}

int stud_tcp_recv(int sockfd, unsigned char *pData, unsigned short datalen, int flags){
    char TAG[100] = "stud_tcp_recv";

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
    return 0;
}

int stud_tcp_close(int sockfd){
    char TAG[100] = "stud_tcp_close";

    map<int, TCB*>::iterator iterator = TCBMap.find(sockfd);
    if (iterator == TCBMap.end() || iterator->second->state != ESTABLISHED){
        if (iterator != TCBMap.end()){
            // state != ESTABLISHED, so it just deletes TCB and returns
            TCBMap.erase(iterator);
            Log(TAG, "delete TCB");
            return 0;
        }
        // wrong socket descriptor
        Log(TAG, "Error 1");
        return -1;
    }
    tcb = iterator->second;

    stud_tcp_output(NULL, 0, PACKET_TYPE_FIN_ACK, tcb->srcPort, tcb->dstPort, tcb->srcAddr, tcb->dstAddr);
    // from ESTABLISHED to FIN_WAIT1

    // wait for response
    TCPHead* thead = new TCPHead();
    int res = waitIpPacket((char*)thead, 5000);
    while (res == -1)
        res = waitIpPacket((char*)thead, 5000);

    if (stud_tcp_input((char*)thead, 20, ntohl(tcb->srcAddr), ntohl(tcb->dstAddr)) == 0){
        // from FIN_WAIT1 to FIN_WAIT2
        delete thead;
        thead = new TCPHead();
        res = waitIpPacket((char*)thead, 5000);
        while (res == -1)
            res = waitIpPacket((char*)thead, 5000);
        if (stud_tcp_input((char*)thead, 20, ntohl(tcb->srcAddr), ntohl(tcb->dstAddr)) == 0){
            // from FIN_WAIT2 to TIME_WAIT
            TCBMap.erase(iterator);
            delete thead;
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