/*
    Author: David Xiang
    Email: xdw@pku.edu.cn
*/

/*
* THIS FILE IS FOR IP TEST
*/

// system support
#include "sysInclude.h"

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
