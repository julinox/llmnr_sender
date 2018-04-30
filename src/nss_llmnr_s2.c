/* Macros */
#define CHECK 1
#define INCLASS 1
#define QUESTMINSZ 17
#define QUERYMAXTRIES 3
#define _GNU_SOURCE
//#define PRIORITY_IPV4

/* Includes */
#include <nss.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/nss_llmnr_netinterface.h"
#include "../include/nss_llmnr_packet.h"
#include "../include/nss_llmnr_responses.h"
#include "../include/nss_llmnr_answers.h"
#include "../include/nss_llmnr_cache.h"
#include "../include/nss_llmnr_s2.h"

/* Enums & Structs */
enum GENERICFLAG {
        _F1 = 1 << 0,
        _F2 = 1 << 1,
        _F3 = 1 << 2,
        _F4 = 1 << 3,
        _F5 = 1 << 4,
        _F6 = 1 << 5,
        _F7 = 1 << 6,
};

typedef struct {
        int fd4;
        int fd6;
        int sndBufSz;
        U_CHAR *sndBuf;
        U_CHAR *rcvBuf;
        U_SHORT id;
        NETIFACE *cIface;
        NETIFACE *ifaces;

} SNDRCVMSG;

/* Prototypes */
PRIVATE int __start(const ARGS *args, NETIFACE *ifs, RESPONSES *resps);
PRIVATE int _O_(const ARGS *args, const SNDRCVMSG *srMsg, RESPONSES *resps);
PRIVATE int recvMsg(const ARGS *args, const SNDRCVMSG *srMsg, RESPONSES *resps);
PRIVATE int recvMsg4(const ARGS *args, const SNDRCVMSG *srMsg, RESPONSES *resps);
PRIVATE int recvMsg6(const ARGS *args, const SNDRCVMSG *srMsg, RESPONSES *resps);
PRIVATE int createPkt(const ARGS *args, U_CHAR *sndBuf);
PRIVATE int createC4Sock();
PRIVATE int createC6Sock();
PRIVATE int setExitIface4(int fd, INADDR *ipv4);
PRIVATE int setExitIface6(int fd, int ifIndex);
PRIVATE int chooseResponse(RESPONSES *resps, ANSWERS *answers, U_SHORT type);
PRIVATE void sendQuery(int fd4, int fd6, U_CHAR *pktBuffer, int pktSz);
PRIVATE void fillMcastDest(SA_IN *dest4, SA_IN6 *dest6);
PRIVATE void fillMcastIp(INADDR *mCast4, IN6ADDR *mCast6);
PRIVATE void *getPktInfo(int family, struct msghdr *msg);
PRIVATE void alertConflict(const SNDRCVMSG *srMsg, RESPONSES *resps);
PRIVATE void flipFlop(U_CHAR c, U_SHORT id, U_CHAR *sndBuf);
PRIVATE void recvTcp(SA *to, const SNDRCVMSG *srMsg, RESPONSES *resps);
PRIVATE void soaAnswer(U_CHAR *buffer, U_SHORT type, ANSWERS *answers);
PRIVATE void getAnswers(U_CHAR *buffer, U_SHORT anCount, ANSWERS *answers);
PRIVATE int _getAnswers(U_SHORT type, U_SHORT rdLen);

/* Glocal variables */

/* Functions definitions */
PUBLIC int startS2(const ARGS *args, NETIFACE *ifs, ANSWERS *answers)
{
        int status;
        RESPONSES *__resps;

        __resps = responsesListHead();
        if (__resps == NULL) {
                *args->errnop = errno;
                *args->herrnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        status = __start(args,ifs,__resps);
        if (status == NSS_STATUS_SUCCESS)
                status = chooseResponse(__resps,answers,args->type);
        if (countAnswers(answers) > 0) {
                cacheAnswers(args->cName,args->type,answers);
        }
        deleteResponsesList(&__resps);
        return status;
}

PRIVATE int __start(const ARGS *args, NETIFACE *ifs, RESPONSES *resps)
{
    /*
     * Send LLMNR querys through all the interfaces allowed.
     * Stop when a valid response is received. If no response
     * received then choose the next interface.
     */
        U_SHORT idd;
        SNDRCVMSG srMsg;
        int status, fd4, fd6;
        U_CHAR *sndBuf, *rcvBuf;

        status = NSS_STATUS_NOTFOUND;
        if (ifs == NULL)
                return status;

        sndBuf = calloc(1,SNDBUFSZ);
        if (sndBuf == NULL) {
                *args->errnop = errno;
                *args->herrnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        rcvBuf = calloc(1,RCVBUFSZ);
        if (rcvBuf == NULL) {
                *args->errnop = errno;
                *args->herrnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        status = 0;
        fd4 = createC4Sock();
        fd6 = createC6Sock();
        memset(&srMsg,0,sizeof(SNDRCVMSG));
        srMsg.fd4 = fd4;
        srMsg.fd6 = fd6;
        srMsg.sndBuf = sndBuf;
        srMsg.rcvBuf = rcvBuf;
        srMsg.cIface = ifs;
        srMsg.ifaces = ifs;
        srMsg.sndBufSz = createPkt(args,srMsg.sndBuf);

        for (; srMsg.cIface != NULL; srMsg.cIface = srMsg.cIface->next) {
                if (srMsg.cIface->flags & _IFF_NOIF)
                        continue;
                srMsg.id = (U_SHORT) random();
                idd = htons(srMsg.id);
                memcpy(srMsg.sndBuf,&idd,sizeof(U_SHORT));
                status = _O_(args,&srMsg,resps);
                if (status == NSS_STATUS_SUCCESS ||
                    status == NSS_STATUS_TRYAGAIN)
                        break;
        }
        if (fd4 > 0)
                close(fd4);
        if (fd6 > 0)
                close(fd6);
        free(sndBuf);
        free(rcvBuf);
        if (countResponses(resps) > 0)
                return NSS_STATUS_SUCCESS;
        return status;
}

PRIVATE int _O_(const ARGS *args, const SNDRCVMSG *srMsg, RESPONSES *resps)
{
    /*
     * - Send LLMNR queries (IPv4 and IPv6 simultaneously)
     * - Check the list 'RESPONSES' and see if any valid response
     *   was received
     * - Check if a conflict was detected and notify it
     */
        U_CHAR flag;
        int count;

        flag = 0;
        count = 0;
        memset(srMsg->rcvBuf,0,RCVBUFSZ);
        if (setExitIface4(srMsg->fd4,getFirstValidAddr(srMsg->cIface)))
                flag |= _F1;
        if (setExitIface6(srMsg->fd6,srMsg->cIface->ifIndex))
                flag |= _F2;
        if (flag & _F1 && flag & _F2) {
                *args->errnop = errno;
                *args->herrnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        while (count < QUERYMAXTRIES) {
                sendQuery(srMsg->fd4,srMsg->fd6,srMsg->sndBuf,srMsg->sndBufSz);
                if (!recvMsg(args,srMsg,resps))
                        break;
                count++;
        }
        if (count >= QUERYMAXTRIES)
                return NSS_STATUS_NOTFOUND;
        if (haveConflict(resps,srMsg->cIface->ifIndex)) {
                alertConflict(srMsg,resps);
                deleteResponsesFrom(srMsg->cIface->ifIndex,resps);
                return NSS_STATUS_NOTFOUND;
        }
        if (!haveValidResponse(resps,srMsg->cIface->ifIndex)) {
                return NSS_STATUS_NOTFOUND;
        }
        return NSS_STATUS_SUCCESS;
}

PRIVATE int recvMsg(const ARGS *args, const SNDRCVMSG *srMsg, RESPONSES *resps)
{
    /*
     * Wait 'LLMNR_TIMEOUT' for potential responses (See RFC 4795)
     * According to a defined priority, first "receive" a pontential
     * response on one socket (either IPv4 or IPv6). If no response
     * then check the other socket
     */
        poll(0,0,LLMNR_TIMEOUT);
        #ifdef PRIORITY_IPV4
                if (!recvMsg4(args,srMsg,resps))
                        return SUCCESS;
                else if (!recvMsg6(args,srMsg,resps))
                        return SUCCESS;
        #else
                if (!recvMsg6(args,srMsg,resps))
                        return SUCCESS;
                else if(!recvMsg4(args,srMsg,resps))
                        return SUCCESS;
        #endif
        return FAILURE;
}

PRIVATE int recvMsg4(const ARGS *args, const SNDRCVMSG *srMsg, RESPONSES *resps)
{
    /*
     * Receive IPv4 responses and store them into a 'RESPONSES' list
     * When 'TC' bit is set in response, repeat the query using TCP
     */
        HEADER head;
        SA_IN from;
        U_CHAR unique;
        struct iovec iov;
        struct msghdr msg;
        RESPONSES response;
        INADDR *myIp, *toIp;
        int rcved, offset, count;
        U_CHAR ancBuffer[ANCBUFSZ];

        if (srMsg->fd4 < 0)
                return FAILURE;
        memset(&iov,0,sizeof(iov));
        memset(&msg,0,sizeof(msg));
        iov.iov_base = srMsg->rcvBuf;
        iov.iov_len = RCVBUFSZ;

        msg.msg_name = &from;
        msg.msg_namelen = sizeof(from);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ancBuffer;
        msg.msg_controllen = ANCBUFSZ;

        count = 0;
        unique = 0;
        while (CHECK) {
                memset(&from,0,sizeof(from));
                memset(srMsg->rcvBuf,0,RCVBUFSZ);
                memset(ancBuffer,0,ANCBUFSZ);
                memset(&head,0,sizeof(HEADER));
                rcved = recvmsg(srMsg->fd4,&msg,MSG_DONTWAIT);
                if (rcved < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                }
                if (rcved < QUESTMINSZ)
                        continue;

                getHeader(srMsg->rcvBuf,&head);
                if (head.ID == 0)
                        return FAILURE;
                if (srMsg->id != head.ID)
                        return FAILURE;
                if (strcasecmp(args->name,(char *)(srMsg->rcvBuf + HEADSZ)))
                        return FAILURE;
                if (head.C && !unique) {
                        poll(0,0,JITTER_INTERVAL);
                        unique = 1;
                }
                if (head.T)
                        return FAILURE;
                toIp = getPktInfo(AF_INET,&msg);
                myIp = getFirstValidAddr(srMsg->cIface);
                if (myIp == NULL || toIp == NULL)
                        return FAILURE;
                if (memcmp(toIp,myIp,sizeof(INADDR)))
                        return FAILURE;
                if (searchIpv4(&from.sin_addr,srMsg->ifaces))
                        return FAILURE;
                if (head.TC) {
                        recvTcp((SA *)&from,srMsg,resps);
                        count++;
                        continue;
                }
                memset(&response,0,sizeof(RESPONSES));
                offset = HEADSZ + strlen(args->name) + 1 + (2 * sizeof(U_SHORT));
                response.buf = srMsg->rcvBuf + offset;
                response.bufLen = rcved - offset;
                response.ip = &from.sin_addr;
                response.ipLen = sizeof(INADDR);
                response.id = head.ID;
                response.cBit = head.C;
                response.anCount = head.ANCOUNT;
                response.fromIface = srMsg->cIface->ifIndex;
                newResponse(&response,resps);
                count++;
        }
        if (count > 0)
                return SUCCESS;
        return FAILURE;
}

PRIVATE int recvMsg6(const ARGS *args, const SNDRCVMSG *srMsg, RESPONSES *resps)
{
    /*
     * Same thing that 'recvMsg4()'
     */
        HEADER head;
        SA_IN6 from;
        U_CHAR unique;
        struct iovec iov;
        struct msghdr msg;
        RESPONSES response;
        IN6ADDR *myIp, *toIp;
        int rcved, offset, count;
        U_CHAR ancBuffer[ANCBUFSZ];

        if (srMsg->fd6 < 0)
                return FAILURE;
        memset(&iov,0,sizeof(iov));
        memset(&msg,0,sizeof(msg));
        iov.iov_base = srMsg->rcvBuf;
        iov.iov_len = RCVBUFSZ;

        msg.msg_name = &from;
        msg.msg_namelen = sizeof(from);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ancBuffer;
        msg.msg_controllen = ANCBUFSZ;

        count = 0;
        unique = 0;
        while (CHECK) {
                memset(&from,0,sizeof(from));
                memset(srMsg->rcvBuf,0,RCVBUFSZ);
                memset(ancBuffer,0,ANCBUFSZ);
                memset(&head,0,sizeof(HEADER));
                rcved = recvmsg(srMsg->fd6,&msg,MSG_DONTWAIT);
                if (rcved < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                }
                if (rcved < QUESTMINSZ)
                        continue;

                getHeader(srMsg->rcvBuf,&head);
                if (head.ID == 0)
                        return FAILURE;
                if (srMsg->id != head.ID)
                        return FAILURE;
                if (strcasecmp(args->name,(char *)(srMsg->rcvBuf + HEADSZ)))
                        return FAILURE;
                if (head.C && !unique) {
                        poll(0,0,JITTER_INTERVAL);
                        unique = 1;
                }
                if (head.T)
                        return FAILURE;

                toIp = getPktInfo(AF_INET6,&msg);
                myIp = getFirstValidAddr6(srMsg->cIface);
                if (myIp == NULL || toIp == NULL)
                        return FAILURE;
                if (memcmp(toIp,myIp,sizeof(IN6ADDR)))
                        return FAILURE;
                if (searchIpv6(&from.sin6_addr,srMsg->ifaces) != NULL)
                        return FAILURE;
                if (head.TC) {
                        recvTcp((SA *)&from,srMsg,resps);
                        count++;
                        continue;
                }
                memset(&response,0,sizeof(RESPONSES));
                offset = HEADSZ + strlen(args->name) + 1 + (2 * sizeof(U_SHORT));
                response.buf = srMsg->rcvBuf + offset;
                response.bufLen = rcved - offset;
                response.ip = &from.sin6_addr;
                response.ipLen = sizeof(IN6ADDR);
                response.id = head.ID;
                response.cBit = head.C;
                response.anCount = head.ANCOUNT;
                response.fromIface = srMsg->cIface->ifIndex;
                newResponse(&response,resps);
                count++;
        }
        if (count > 0)
                return SUCCESS;
        return FAILURE;
}

PRIVATE void recvTcp(SA *to, const SNDRCVMSG *srMsg, RESPONSES *resps)
{
    /*
     * LLMNR TCP query. Save response into 'RESPONSES' list
     */
        size_t len;
        U_CHAR *rcvBuf;
        HEADER head;
        RESPONSES response;
        int fd, rcved, n, offset;

        n = 0;
        fd = 0;
        len = 0;
        rcved = 0;
        offset = 0;
        rcvBuf = calloc(1,TCPBUFSZ);
        if (rcvBuf == NULL)
                return;

        memset(&head,0,sizeof(HEADER));
        memset(&response,0,sizeof(RESPONSES));
        switch (to->sa_family) {
        case AF_INET:
                fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
                len = sizeof(SA_IN);
                response.ip = &((SA_IN *)to)->sin_addr;
                break;
        case AF_INET6:
                fd = socket(AF_INET6,SOCK_STREAM,IPPROTO_TCP);
                len = sizeof(SA_IN6);
                response.ip = &((SA_IN6 *)to)->sin6_addr;
                break;
        default:
                return;
        }
        if (connect(fd,to,len))
                goto FYN;
        if (send(fd,srMsg->sndBuf,srMsg->sndBufSz,0) < 0)
                goto FYN;

        while (CHECK) {
                n = recv(fd,rcvBuf,TCPBUFSZ,0);
                if (n < 0)
                        goto FYN;
                else if (n == 0)
                        break;
                rcved += n;
        }
        getHeader(rcvBuf,&head);
        offset = HEADSZ + strlen((char *) (rcvBuf + HEADSZ)) + 1 +
                (2 * sizeof(U_SHORT));
        response.buf = rcvBuf + offset;
        response.bufLen = rcved - offset;
        response.ipLen = len;
        response.id = head.ID;
        response.cBit = head.C;
        response.anCount = head.ANCOUNT;
        response.fromIface = srMsg->cIface->ifIndex;
        newResponse(&response,resps);

        FYN:
        free(rcvBuf);
        close(fd);
}

PRIVATE int createPkt(const ARGS *args, U_CHAR *sndBuf)
{
    /*
     * "Creates" a LLMNR query packet
     */
        int pktSz;
        HEADER head;
        QUERY query;

        pktSz = 0;
        memset(&head,0,sizeof(HEADER));
        memset(&query,0,sizeof(QUERY));
        head.ID = 0;
        head.QDCOUNT = 1;
        query.QNAME = args->name;
        query.QTYPE = args->type;
        query.QCLASS = INCLASS;
        pktSz += attachHeader(&head,sndBuf);
        pktSz += attachQuery(&query,sndBuf + HEADSZ);
        return pktSz;
}

PRIVATE int createC4Sock()
{
    /*
     * Creates an IPv4 socket for sending multicast (to 224.0.0.252)
     * packets. This sockets is used when resolving conflicts
     * IP_MUlTICAST_LOOP disabled.
     */
        int fd;
        SA_IN bindd;
        unsigned int on;

        fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (fd < 0)
                return FAILURE;
        on = 0;
        setsockopt(fd,IPPROTO_IP,IP_MULTICAST_LOOP,(void *)&on,
                       sizeof(unsigned int));
        on = 1;
        setsockopt(fd,IPPROTO_IP,IP_MULTICAST_TTL,(void *)&on,
                       sizeof(unsigned int));
        if (setsockopt(fd,IPPROTO_IP,IP_PKTINFO,(void *)&on,
                       sizeof(unsigned int))) {
                close(fd);
                return FAILURE;
        }

        memset(&bindd,0,sizeof(bindd));
        bindd.sin_family = AF_INET;
        bindd.sin_port = 0;
        if (bind(fd,(SA *)&bindd,sizeof(bindd))) {
                close(fd);
                return FAILURE;
        }
        return fd;
}

PRIVATE int createC6Sock()
{
    /*
     * Same thing that 'createC4Sock'
     */

        int fd;
        SA_IN6 bindd;
        unsigned int on;

        fd = socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);
        if (fd < 0)
                return FAILURE;
        on = 0;
        setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_LOOP,(void *)&on,
                   sizeof(unsigned int));
        on = 1;
        setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_HOPS,(void *)&on,
                   sizeof(int));
        setsockopt(fd,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&on,
                   sizeof(unsigned int));
        if (setsockopt(fd,IPPROTO_IPV6,IPV6_RECVPKTINFO,(void *)&on,
                       sizeof(unsigned int)))
                return FAILURE;
        memset(&bindd,0,sizeof(bindd));
        bindd.sin6_family = AF_INET6;
        bindd.sin6_port = 0;
        if (bind(fd,(SA *)&bindd,sizeof(bindd))) {
                close(fd);
                return FAILURE;
        }
        return fd;
}

PRIVATE int setExitIface4(int fd, INADDR *ipv4)
{
    /*
     * Set the exit interface for the LLMNR IPv4 query
     */
        if (ipv4 == NULL || fd < 0)
                return FAILURE;
        if (setsockopt(fd,IPPROTO_IP,IP_MULTICAST_IF,(void *)ipv4,
                    sizeof(INADDR))) {
                close(fd);
                return FAILURE;
        }
        return SUCCESS;
}

PRIVATE int setExitIface6(int fd, int ifIndex)
{
    /*
     * Set the exit interface for the LLMNR IPv6 query
     */
        if (fd < 0)
                return FAILURE;
        if (setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_IF,(void *)&ifIndex,
                    sizeof(unsigned int)))
                return FAILURE;
        return SUCCESS;
}

PRIVATE void sendQuery(int fd4, int fd6, U_CHAR *pktBuffer, int pktSz)
{
    /*
     * send a packet to multicast destiny (224.0.0.252 and FF::1:3)
     */
        SA_IN dest4;
        SA_IN6 dest6;

        fillMcastDest(&dest4,&dest6);
        if (fd4 > 0)
                sendto(fd4,pktBuffer,pktSz,0,(SA *)&dest4,sizeof(SA_IN));

        if (fd6 > 0)
                sendto(fd6,pktBuffer,pktSz,0,(SA *)&dest6,sizeof(SA_IN6));
}

PRIVATE void fillMcastDest(SA_IN *dest4, SA_IN6 *dest6)
{
    /*
     * Fill an sockaddr_in and sockaddr_in6 structure
     * to send a multicast message
     */
        if (dest4 != NULL) {
                memset(dest4,0,sizeof(SA_IN));
                dest4->sin_family = AF_INET;
                dest4->sin_port = htons(LLMNRPORT);
                fillMcastIp(&dest4->sin_addr,NULL);
        }
        if (dest6 != NULL) {
                memset(dest6,0,sizeof(SA_IN6));
                dest6->sin6_family = AF_INET6;
                dest6->sin6_port = htons(LLMNRPORT);
                fillMcastIp(NULL,&dest6->sin6_addr);
        }
}

PRIVATE void fillMcastIp(INADDR *mCast4, IN6ADDR *mCast6)
{
    /*
     * Fill the global variables MCAST_IPv4 and
     * MCAST_IPv6 (224.0.0.252 & FF02::1:3)
     */

        if (mCast4 != NULL)
                mCast4->s_addr = htonl(0xE00000FC);
        if (mCast6 != NULL) {
                memset(&mCast6->s6_addr,0,sizeof(struct in6_addr));
                mCast6->s6_addr[0] = 0xFF;
                mCast6->s6_addr[1] = 0x2;
                mCast6->s6_addr[13] = 0x1;
                mCast6->s6_addr[15] = 0x3;
        }
}
PRIVATE void *getPktInfo(int family, struct msghdr *msg)
{
    /*
     * Get the destiny IP addres from a received response. i.e
     * to see if the response was for "me"
     */
        void *ret;
        struct cmsghdr *cmsgPtr;
        struct in_pktinfo *pktInfo4;
        struct in6_pktinfo *pktInfo6;

        ret = NULL;
        cmsgPtr = CMSG_FIRSTHDR(msg);
        for (; cmsgPtr != NULL; cmsgPtr = CMSG_NXTHDR(msg,cmsgPtr)) {
                if (family == AF_INET && cmsgPtr->cmsg_level == IPPROTO_IP
                    && cmsgPtr->cmsg_type == IP_PKTINFO) {
                        pktInfo4 = (struct in_pktinfo *) CMSG_DATA(cmsgPtr);
                        ret = (void *) &pktInfo4->ipi_addr;

                } else if (family == AF_INET6
                           && cmsgPtr->cmsg_level == IPPROTO_IPV6
                           && cmsgPtr->cmsg_type == IPV6_PKTINFO) {
                        pktInfo6 = (struct in6_pktinfo*) CMSG_DATA(cmsgPtr);
                        ret = (void *) &pktInfo6->ipi6_addr;
                }
        }
        return ret;
}

PRIVATE void alertConflict(const SNDRCVMSG *srMsg, RESPONSES *resps)
{
    /*
     * Checks from which interface the "conflicting" responses were
     * received. Then send the conflict alert LLMNR query
     */
        U_CHAR flag;
        int fd4, fd6;
        RESPONSES *current;

        fd4 = 0;
        fd6 = 0;
        flag = 0;
        current = resps;
        for (; current != NULL; current = current->next) {
                if (current->id == 0)
                        continue;
                if (current->fromIface != srMsg->cIface->ifIndex)
                        continue;
                if (current->ipLen == sizeof(INADDR))
                        flag |= _F4;
                else if (current->ipLen == sizeof(IN6ADDR))
                        flag |= _F6;
        }
        if (flag &_F4)
                fd4 = srMsg->fd4;
        if (flag & _F6)
                fd4 = srMsg->fd4;
        flipFlop(1,srMsg->id,srMsg->sndBuf);
        sendQuery(fd4,fd6,srMsg->sndBuf,srMsg->sndBufSz);
        flipFlop(0,srMsg->id,srMsg->sndBuf);
}

PRIVATE void flipFlop(U_CHAR c, U_SHORT id, U_CHAR *sndBuf)
{
    /*
     * Activate 'C' bit in a given LLMNR query (store in 'sndBuf')
     */
        HEADER head;

        memset(&head,0,sizeof(HEADER));
        head.ID = id;
        head.C = c;
        head.QDCOUNT = 1;
        attachHeader(&head,sndBuf);
}

PRIVATE int chooseResponse(RESPONSES *resps, ANSWERS *answers, U_SHORT type)
{
    /*
     * If im here then at least i have one response
     * Type needed when SOA response received. i.e
     * have response when
     */
        int status;
        RESPONSES *current;

        if (resps == NULL || countResponses(resps) < 1)
                return NSS_STATUS_NOTFOUND;

        current = resps;
        for (; current != NULL; current = current->next) {
                if (current->anCount >= 1)
                        break;
        }
        if (current == NULL) {
                if (resps->next != NULL)
                        soaAnswer(resps->next->buf,type,answers);
                status = NSS_STATUS_NOTFOUND;

        } else {
                getAnswers(current->buf,current->anCount,answers);
                status = NSS_STATUS_SUCCESS;
        }
        return status;
}

PRIVATE void soaAnswer(U_CHAR *buffer, U_SHORT type, ANSWERS *answers)
{
    /*
     * Handle a 'SOA' response
     */
        int ttl;
        U_CHAR *answer;
        U_SHORT offset;

        offset = 0;
        ttl = RRTTL;
        answer = buffer;

        if (*answer == 0xC0)
                offset += 2;
        else
                offset  += strlen((char *)answers) + 1;
        offset += 2 * sizeof(U_SHORT);
        ttl = ntohl(*((int *)(answer + offset)));
        offset = 0;
        newAnswer(answers,type,ttl,&offset,0);
}

PRIVATE void getAnswers(U_CHAR *buffer, U_SHORT anCount, ANSWERS *answers)
{
    /*
     * Process all answers and store them (by type) into a 'ANSWERS' list
     */
        int i, ttl;
        U_CHAR  *answer, *rData;
        U_SHORT offset, type, rdLen;

        answer = buffer;
        for (i = 0; i < anCount; i++) {
                offset = 0;

                if (*answer == 0xC0)
                        offset += 2;
                else
                        offset += strlen((char *)answer) + 1;
                type = ntohs(*((U_SHORT *)(answer + offset)));
                offset += sizeof(U_SHORT) + sizeof(U_SHORT);
                ttl = ntohl(*((int *)(answer + offset)));
                offset += sizeof(int);
                rdLen = ntohs(*((U_SHORT *)(answer + offset)));
                offset += sizeof(U_SHORT);
                rData = answer + offset;
                answer += offset + rdLen;
                if (_getAnswers(type,rdLen))
                        continue;
                newAnswer(answers,type,ttl,rData,rdLen);
        }
}

PRIVATE int _getAnswers(U_SHORT type, U_SHORT rdLen)
{
    /*
     * Helper for 'getAnswers()'
     */
        switch (type) {
        case A:
                if (rdLen != IPV4LEN)
                        return FAILURE;
                break;
        case AAAA:
                if (rdLen != IPV6LEN)
                        return FAILURE;
                break;
        case PTR:
                if (rdLen > HOSTNAMEMAX)
                        return FAILURE;
                break;
        }
        return SUCCESS;
}
