/* Macros */
#define _GNU_SOURCE
#define HEADERFIELDS 6
#define HEADERFLAGS 10

/* Includes */
#include <string.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/nss_llmnr_netinterface.h"
#include "../include/nss_llmnr_packet.h"

/* Enums & Structs */
typedef union {
        struct cmsghdr hdr;
        U_CHAR buff[CMSG_SPACE(sizeof(struct in_pktinfo))];
} IPV4CMSG;

/* Private functions prototypes */
PRIVATE int _sendPacket(PKTSND *pktSnd);
PRIVATE int testEndianness();
PRIVATE void copyShortValues(U_CHAR *dest, U_CHAR *values, int valuesSz);
PRIVATE U_SHORT flagsToShortValue(U_CHAR *flags);

/* Glocal variables */

/* Functions definitions */
PUBLIC void getHeader(U_CHAR *buffer, HEADER *head)
{
    /*
     * Given a buffer, extract the LLMNR header within
     * and store it into a 'HEADER' struct
     */
        U_CHAR byte1, byte2;

        if (head == NULL)
                return;
        byte1 = *((U_CHAR *)buffer + 2);
        byte2 = *((U_CHAR *)buffer + 3);
        head->ID = ntohs(*((U_SHORT *)buffer));
        head->QR = ((0x1 << 7) & byte1) >> 7;
        head->OPCODE = ((0xF << 3) & byte1) >> 3;
        head->C = ((0x1 << 2) & byte1) >> 2;
        head->TC = ((0x1 << 1) & byte1) >> 1;
        head->T = 0x1 & byte1;
        head->Z0 = ((0x1 << 7) & byte2) >> 7;
        head->Z1 = ((0x1 << 6) & byte2) >> 6;
        head->Z2 = ((0x1 << 5) & byte2) >> 5;
        head->Z3 = ((0x1 << 4) & byte2) >> 4;
        head->RCODE = 0xF & byte2;
        head->QDCOUNT = ntohs(*((U_SHORT *)(buffer + 4)));
        head->ANCOUNT = ntohs(*((U_SHORT *)(buffer + 6)));
        head->NSCOUNT = ntohs(*((U_SHORT *)(buffer + 8)));
        head->ARCOUNT = ntohs(*((U_SHORT *)(buffer + 10)));
}

PUBLIC void getQuery(U_CHAR *buffer, QUERY *query)
{
    /*
     * Given a buffer, extract the LLMNR query within
     * and store it into a 'QUERY' struct
     */
        query->QNAME = (char *)buffer + HEADSZ;
        query->QTYPE = *((U_SHORT *)(buffer + HEADSZ +
                          strlen(query->QNAME) + 1));
        query->QTYPE = ntohs(query->QTYPE);
        query->QCLASS = *((U_SHORT *)(buffer + HEADSZ +
                           strlen(query->QNAME) + 1 + sizeof(U_SHORT)));
        query->QCLASS = ntohs(query->QCLASS);
}

PUBLIC int attachHeader(HEADER *header, U_CHAR *pktBuff)
{
    /*
     * Given a 'HEADER' struct, "decompose" it and store it into
     * 'pktBuff'. Returns the number of bytes copied into 'pktBuff'
     * in this case will always be 'HEADSZ'
     */
        U_CHAR headerFlags[HEADERFLAGS];
        U_SHORT headerValues[HEADERFIELDS];

        memset(headerFlags,0,HEADERFLAGS);
        memset(headerValues,0,HEADERFIELDS);
        headerFlags[0] = header->QR ;
        headerFlags[1] = header->OPCODE;
        headerFlags[2] = header->C;
        headerFlags[3] = header->TC;
        headerFlags[4] = header->T;
        headerFlags[5] = header->Z0;
        headerFlags[6] = header->Z1;
        headerFlags[7] = header->Z2;
        headerFlags[8] = header->Z3;
        headerFlags[9] = header->RCODE;
        headerValues[0] = header->ID;
        headerValues[1] = flagsToShortValue(headerFlags);
        headerValues[2] = header->QDCOUNT;
        headerValues[3] = header->ANCOUNT;
        headerValues[4] = header->NSCOUNT;
        headerValues[5] = header->ARCOUNT;
        copyShortValues(pktBuff,(U_CHAR *)headerValues,HEADERFIELDS);
        return HEADSZ;
}

PUBLIC int attachQuery(QUERY *query, U_CHAR *pktBuff)
{
    /*
     * Given a 'QUERY' struct, "decompose" it and store it into
     * 'pktBuff'. Returns the numbers of bytes copied into 'pktBuff'
     * in this case: strlen of QNAME + QTYPE (2 bytes) + QCLASS (2 bytes)
     * Returns the number of bytes wrote into the buffer
     */
        int pktSz;
        U_SHORT aux;
        int u_shortSz;

        pktSz = 0;
        u_shortSz = sizeof(U_SHORT);
        strcpy((char *)pktBuff,query->QNAME);
        pktSz = strlen(query->QNAME) + 1;
        aux = htons(query->QTYPE);
        memcpy(pktBuff + pktSz,&aux,u_shortSz);
        pktSz += u_shortSz;
        aux = htons(query->QCLASS);
        memcpy(pktBuff + pktSz,&aux,u_shortSz);
        pktSz += u_shortSz;
        return pktSz;
}

PUBLIC int sendUDPacket(PKTSND *pktSnd)
{
    /*
     * A regular sendto, except that when the layer 3
     * protocol is IPv4 then sendmsg is used to force
     * the exit interface. When IPv6 is the layer 3
     * protocol choosing an exit interface does not work.
     * Apparently the kernel will ignore it and choose
     * his own based on the destiny's IPv6 kind
     * Returns the number of bytes sent
     */
        int sent;

        sent = 0;
        if (pktSnd->to->sa_family == AF_INET)
                sent = _sendPacket(pktSnd);
        else if (pktSnd->to->sa_family == AF_INET6)
                sent = sendto(pktSnd->fd,pktSnd->pktBuff,pktSnd->pktSz,0,
                              pktSnd->to,sizeof(SA_IN6));
        return sent;
}

PUBLIC int sendTCPacket(PKTSND *pktSnd)
{
    /*
     * Returns the number of bytes sent
     */
    int n, total, bytesLeft;

    n = 0;
    total = 0;
    bytesLeft = pktSnd->pktSz;
    while (total < pktSnd->pktSz) {
            n = send(pktSnd->fd,pktSnd->pktBuff + total,bytesLeft,0);
            if (n < 0)
                    break;
            total += n;
            bytesLeft -= n;
    }
    return total;
}

PRIVATE int _sendPacket(PKTSND *pktSnd)
{
    /*
     * Using recvmsg() to force the exit interface. This
     * only works when the layer 3 protocol is IPV4
     * Returns the number of bytes sent
     */
        int sent;
        void *vPtr;
        IPV4CMSG ancData;
        struct iovec iov;
        struct msghdr msg;
        struct cmsghdr *cmsgPtr;
        struct in_pktinfo pktInfo;

        sent = 0;
        memset(&iov,0,sizeof(iov));
        memset(&msg,0,sizeof(msg));
        memset(&ancData,0,sizeof(ancData));
        memset(&pktInfo,0,sizeof(pktInfo));
        iov.iov_base = pktSnd->pktBuff;
        iov.iov_len = pktSnd->pktSz;
        msg.msg_name = (void *)pktSnd->to;
        msg.msg_namelen = sizeof(SA_IN);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ancData.buff;
        msg.msg_controllen = sizeof(ancData.buff);
        pktInfo.ipi_ifindex = pktSnd->ifIndex;
        cmsgPtr = CMSG_FIRSTHDR(&msg);
        cmsgPtr->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
        cmsgPtr->cmsg_level = IPPROTO_IP;
        cmsgPtr->cmsg_type = IP_PKTINFO;
        vPtr = (void *)CMSG_DATA(cmsgPtr);
        memcpy(vPtr,&pktInfo,sizeof(pktInfo));
        sent = sendmsg(pktSnd->fd,&msg,0);
        if (sent < 0)
                sent = sendto(pktSnd->fd,pktSnd->pktBuff,pktSnd->pktSz,
                              0,(SA *)pktSnd->to,sizeof(SA_IN));
        return sent;
}

PRIVATE U_SHORT flagsToShortValue(U_CHAR *flags)
{
    /*
     * This function builds the corresponding short value
     * regarding the LLMNR header flags position
     */

        U_SHORT aux , final;
        U_CHAR i, sizeActual;

        final = 0;
        sizeActual = 16;

        for (i=0; i < 10; i++) {
                if (i==1 || i==9) {
                        sizeActual = sizeActual - 4;
                        flags[i] = flags[i] & ((1 << 4) - 1);

                        if (i==9)
                                sizeActual = 0;
                } else {
                        sizeActual--;
                        flags[i] = flags[i] & 1;
                }
                aux = flags[i] << sizeActual;

                if (i==0)
                        final = aux;
                else
                        final = final | aux;
        }
        return final;
}

PRIVATE void copyShortValues(U_CHAR *dest, U_CHAR *values, int valuesSz)
{
    /*
     * Copy a string of short values into a buffer
     * This "string" of short values holds the values
     * related to a LLMNR 'HEADER'
     */
        int i, offset, u_shortSz;

        u_shortSz = sizeof(U_SHORT);
        if (testEndianness()) {
                offset = valuesSz * u_shortSz;
                memcpy(dest,values,offset);
        }
        for (i = 0; i < valuesSz * u_shortSz; i = i + u_shortSz) {
                dest[i] = values[i + 1];
                dest[i + 1] = values[i];
        }
}

PRIVATE int testEndianness()
{
    /** ********************
    * 0 --> Little-Endian *
    * !0 --> Big-Endian   *
    ***********************/
        char *ptr;
        unsigned int test;

        test = 0xFF;
        ptr = (char *)&test;
        if (ptr != 0x0)
                return SUCCESS;
        else
                return FAILURE;
}
