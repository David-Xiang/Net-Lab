# 网络上机报告：IPv4协议收发实验

## 概述

本实验要求我们实现互联网协议栈中网络层IP协议中的IPv4协议，主要需要完成主机在网络层实现接收/发送IPv4网络分组的功能，即`stud_ip_recv()`和`stud_ip_Upsend`函数的功能，其中最关键的问题，我认为是**IP分组的校验和计算以及字节序的转换问题**。

## 个人实现

有关IP协议的具体内容，在指导手册和Tanenbaum的教材上已经做了详细的讲解，因此我在这里不在做赘述，直接进入个人的实现部分。

### 校验和函数

在多方比较之后，我选择的了一个非常简洁的校验和工具函数。具体算法是，在IP分组头部计算校验和时，跳过存放校验和的位置（i=5的位置）。这样计算出来的取反校验和可以直接存放。做检验的时候，也是计算其他部分的校验和之后了，与数据分组发过来的校验和做比较。因此，本函数可以十分简洁：

```c++
unsigned short int getHeaderChecksum(char *pBuffer)
{
    int sum = 0;
    for(int i = 0; i < 10; ++i)
        if(i != 5) // skip the position for checksum
            sum += ((unsigned short*)pBuffer)[i];
    sum = (sum & 0xffff) + (sum >> 16); 
    return ~sum;
}
// 发送示例:
// unsigned short checksum = getHeaderChecksum(pkt);
// memcpy(pkt+10, &checksum, sizeof(unsigned short));
// 
// 检验示例：
// unsigned int headerChecksum = *((unsigned short int*) (pBuffer + 10));
// if (headerChecksum != getHeaderChecksum(pBuffer)){
//        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
//        return 1;
// }
```

### 字节序转换

在通常的X86/64计算机上，数据通常按照小端法存储：即大于1字节的数据会按照地址从低到高，分别存放，表示位数从小到大的字节。而在互联网协议中，数据通常是以大端法进行传递的，因此我们需要将一些数据的存放顺序做一个转换。

对那些数据需要做转换呢？大于1字节的数据。包括short int/int/long int等，需要用`htons`/`htonl`函数做一转换。

### `stud_ip_recv()`

首先在IP分组中提取出相关需要校验的信息，包括：

1. IP协议：需等于4
2. 头部长度：需等于5
3. 存活时间：需大于0
4. 头部校验
5. 目标地址：需要为本机地址或者为广播地址

如果不满足以上条件，则丢弃IP分组，返回1。如满足，则交由上一层处理，返回0。

### `stud_ip_Upsend`

首先需要按照数据长度与首部长度的和申请分组空间，再在申请空间中填入协议版本、头部长度，分组长度，存活时间、协议、源地址、目的地址。然后计算校验和，填入对应位置，填入数据部分。交由下一层处理。

## 源代码

```c
/*
    Author: David Xiang
    Email: xdw@pku.edu.cn
*/

extern void ip_DiscardPkt(char* pBuffer,int type);

extern void ip_SendtoLower(char*pBuffer,int length);

extern void ip_SendtoUp(char *pBuffer,int length);

extern unsigned int getIpv4Address();

// implemented by students
unsigned short int getHeaderChecksum(char *pBuffer)
{
    int sum = 0;
    for(int i = 0; i < 10; ++i)
        if(i != 5) // skip the position for checksum
            sum += ((unsigned short*)pBuffer)[i];
    sum = (sum & 0xffff) + (sum >> 16); 
    return ~sum;
}

int stud_ip_recv(char *pBuffer,unsigned short length)
{
    // check fields of IPv4 header
    unsigned int version = (unsigned int) pBuffer[0] >> 4; // Version
    unsigned int headerLength = (unsigned int) (pBuffer[0] & 0xf); // Header length
    unsigned int timeToLive = (unsigned int) pBuffer[8]; // Time to live
    unsigned int headerChecksum = *((unsigned short int*) (pBuffer + 10)); // Header checksum
    unsigned int dstAddress = ntohl(*((unsigned int*) (pBuffer + 16))); // Destination address

    if (version != 4){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
        return 1;
    } else if (headerLength != 5){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
        return 1;
	} else if (timeToLive <= 0){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
        return 1;
    } else if (headerChecksum != getHeaderChecksum(pBuffer)){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return 1;
    } else if (dstAddress != getIpv4Address() && dstAddress != 0xffffffff){
        // if this pkt is not sent to us(local ip or broadcast), call ip_DiscardPkt()
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
        return 1;
    }

    // if this pkt is sent to this device, call ip_SendtoUp()
    ip_SendtoUp(pBuffer, length);
    return 0; // successfully send this pkt and handle it over to upper layer
}

int stud_ip_Upsend(char *pBuffer,unsigned short len,unsigned int srcAddr,
                   unsigned int dstAddr,byte protocol,byte ttl)
{
    // use param len to request memory for pkt
    int totalLength = sizeof(char) * (len+20);
    char* pkt = (char *) malloc(totalLength);

    unsigned short htonsTotalLength = htons(totalLength);
    unsigned int htonlSrcAddr = htonl(srcAddr);
    unsigned int htonlDstAddr = htonl(dstAddr);

    // fill each field
    pkt[0] = 0x45; // Version and IHL
    memcpy(pkt+2, &htonsTotalLength, sizeof(unsigned short)); // Total length
    pkt[8] = ttl; // TTL
    pkt[9] = protocol; // Protocol
    memcpy(pkt+12, &htonlSrcAddr, sizeof(unsigned int)); // Src address
    memcpy(pkt+16, &htonlDstAddr, sizeof(unsigned int)); // Dst address
    
    unsigned short checksum = getHeaderChecksum(pkt);
    memcpy(pkt+10, &checksum, sizeof(unsigned short)); // checksum

    memcpy(pkt + 20, pBuffer, len); // data
    ip_SendtoLower(pkt, totalLength);
    return 0;
}
```

