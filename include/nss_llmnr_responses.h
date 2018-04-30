/** *************************************************
 * Interface to store all LLMNR responses received  *
 * from a query previously made. A 'RESPONSES' list *
 * will contain the responses                       *
 ****************************************************/

#ifndef NSS_LLMNR_RESPONSES_H
#define NSS_LLMNR_RESPONSES_H
#include "../include/nss_llmnr_defs.h"

typedef struct resps{
        void *ip;
        U_CHAR *buf;
        U_CHAR cBit;
        U_SHORT id;
        size_t ipLen;
        U_SHORT bufLen;
        U_SHORT anCount;
        int fromIface;
        struct resps *next;
} RESPONSES;

PUBLIC RESPONSES *responsesListHead();
PUBLIC int newResponse(RESPONSES *newRes, RESPONSES *list);
PUBLIC void deleteResponsesList(RESPONSES **list);
PUBLIC void deleteResponsesFrom(int ifIndex, RESPONSES *list);
PUBLIC void printResponses(RESPONSES *list);
PUBLIC void printResponse(RESPONSES *response);
PUBLIC int countResponses(RESPONSES *list);
PUBLIC U_CHAR haveConflict(RESPONSES *list, int ifIndex);
PUBLIC U_CHAR haveValidResponse(RESPONSES *list, int ifIndex);
#endif
