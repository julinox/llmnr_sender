/** ******************************************************************
 * Interface to handle the construction and sending of LLMNR packets *
 *********************************************************************/

#ifndef NSS_LLMNR_PACKET_H
#define NSS_LLMNR_PACKET_H
#include "../include/nss_llmnr_defs.h"

/* Enums & Structs */
typedef struct {
        unsigned short ID;
        unsigned char QR;
        unsigned char OPCODE;
        unsigned char C;
        unsigned char TC;
        unsigned char T;
        unsigned char Z0;
        unsigned char Z1;
        unsigned char Z2;
        unsigned char Z3;
        unsigned char RCODE;
        unsigned short QDCOUNT;
        unsigned short ANCOUNT;
        unsigned short NSCOUNT;
        unsigned short ARCOUNT;
} HEADER;

typedef struct {
        const char *QNAME;
        unsigned short QTYPE;
        unsigned short QCLASS;
} QUERY;

typedef struct {
        int fd;
        int pktSz;
        int ifIndex;
        U_CHAR *pktBuff;
        SA *to;
} PKTSND;

/*
 * struct to synthetize a list of function parameters
 */
typedef struct {
        HEADER *head;
        QUERY *query;
        int ipType;
        int rcvIface;
        U_CHAR *pktBuff;
        U_SHORT *namePtr;
} PKTPARAMS;

PUBLIC void printHead(HEADER *head);
PUBLIC void getHeader(U_CHAR *buffer, HEADER *head);
PUBLIC void getQuery(U_CHAR *buffer, QUERY *query);
PUBLIC int attachHeader(HEADER *header, U_CHAR *pktBuff);
PUBLIC int attachQuery(QUERY *query, U_CHAR *pktBuff);
PUBLIC int sendUDPacket(PKTSND *pktSnd);
PUBLIC int sendTCPacket(PKTSND *pktSnd);
#endif
