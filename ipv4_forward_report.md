# 网络上机报告：IPv4协议转发实验

## 概述

本实验要求我们实现互联网协议栈中网络层IP协议中的IPv4协议，主要需要完成作为计算机网络中间节点的路由器的分组转发功能，其中最关键的问题，是路由表的设计以及查验路由表实现分组转发的两个功能。

## 个人实现

有关路由器分组转发IP协议的具体内容，在指导手册和Tanenbaum的教材上已经做了详细的讲解，因此我在这里不在做赘述，直接进入个人的实现部分。

### 路由表实现

路由表为一个结构体数组，由C++的STL库vector实现动态分配大小。结构体的具体内容为：

```C++
struct Record{
	unsigned int dest;
	unsigned int masklen;
	unsigned int nexthop;
};
```

代表一条路由表记录的目的地址，子网掩码长度，下一跳地址。在`stud_route_add`添加路由表记录时，需要将每一个字段转换大小端序。

### 转发过程

首先根据IPv4协议，从pBuffer中读出目的地址、TTL和头部长度等信息。如果目的地址等于本机地址，则直接交由上一层处理。如果TTL小于等于0，则不需要在转交给路径下一跳，直接丢弃分组。

此时需要遍历路由表，找出最长匹配的路由表项。在我的设计中，具体就是找出匹配的子网掩码长度最短的表项。则将当前分组转发给对应表项中记录的下一跳地址。在转发之前，需要将分组的TTL-1以及重新计算头部校验和。

## 源代码

```c
/*
    Author: David Xiang
    Email: xdw@pku.edu.cn
*/

/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include <vector>
using namespace std;

// system support
extern void fwd_LocalRcv(char *pBuffer, int length);

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char *pBuffer, int type);

extern unsigned int getIpv4Address( );

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

struct Record{
	unsigned int dest;
	unsigned int masklen;
	unsigned int nexthop;
};

vector<Record> routeTable;

void stud_Route_Init()
{
	return;
}

void stud_route_add(stud_route_msg *proute)
{
	Record r;
	r.dest = htonl(proute->dest);
	r.masklen = htonl(proute->masklen);
	r.nexthop = htonl(proute->nexthop);
	routeTable.push_back(r);
	return;
}


int stud_fwd_deal(char *pBuffer, int length)
{
	unsigned int dstAddress = ntohl(*((unsigned int*) (pBuffer + 16)));
	unsigned int timeToLive = (int) pBuffer[8]; // Time to live
	unsigned int headerLength = (int) (pBuffer[0] & 0xf); // Header length

	int localAddress = getIpv4Address();
	if (dstAddress == localAddress){
		fwd_LocalRcv(pBuffer, length);
		return 0;
	}

	if (timeToLive <= 0){
		// Error: TimeToLive is wrong
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
		return 1;
	}

	unsigned int minmasklen = 0xffffffff;
	unsigned int nexthop = 0;
	int i = 0;
	for (vector<Record>::iterator it = routeTable.begin(); it !=routeTable.end(); it++){
		unsigned int masklen = (*it).masklen;
		unsigned int mask = 0xffffffff << (32 - masklen);
		//printf("mask=%X\t, dstAddress=%X, it->dest=%X\n", mask, dstAddress, (*it).dest);
		if ((dstAddress & mask == (*it).dest & mask) && (masklen < minmasklen)){
			minmasklen = masklen;
			nexthop = (*it).nexthop;
		}
	}
	
	if (minmasklen == 0xffffffff){
		// Error: didn't find a match
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
		return 1;
	}

	char* pBufferNew = (char*) malloc(length);
	memcpy(pBufferNew, pBuffer, length);
	buffer[8] = buffer[8] - 1;

	unsigned short checksum = getHeaderChecksum(pBufferNew);
    memcpy(pBufferNew+10, &checksum, sizeof(unsigned short)); // checksum
	fwd_SendtoLower(pBuffer, length, nexthop);
	return 0;
}


```

