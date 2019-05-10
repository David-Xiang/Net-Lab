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

