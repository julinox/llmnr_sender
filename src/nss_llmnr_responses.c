/* Macros */

/* Includes */
#include <stdlib.h>
#include <string.h>

/* Own includes */
#include "../include/nss_llmnr_print.h"
#include "../include/nss_llmnr_responses.h"

/* Enums & Structs */

/* Glocal variables */


/* Private prototypes */
PRIVATE int countResponses2(RESPONSES *list, int ifIndex);

/* Functions definitions */
PUBLIC RESPONSES *responsesListHead()
{
        RESPONSES *head;

        head = calloc(1,sizeof(RESPONSES));
        if (head == NULL)
                return NULL;
        head->ip = NULL;
        head->cBit = 0;
        head->buf = NULL;
        head->id = 0;
        head->ipLen = 0;
        head->bufLen = 0;
        return head;
}

PUBLIC int newResponse(RESPONSES *newRes, RESPONSES *list)
{
    /*
     * Add new responses to the list. Duplicate responses
     * are discarded (checked by id and ip)
     */
        void *ipAux;
        U_CHAR *bufAux;
        RESPONSES *newNode, *current;

        if (newRes == NULL || list == NULL)
                return SUCCESS;
        if (newRes->ip == NULL || newRes->buf == NULL)
                return SUCCESS;
        current = list;
        while (current != NULL) {
                if (current->id == newRes->id &&
                    !memcmp(current->ip,newRes->ip,newRes->ipLen))
                        return SUCCESS;
                if (current->next == NULL)
                        break;
                current = current->next;
        }
        ipAux = calloc(1,newRes->ipLen);
        bufAux = calloc(1,newRes->bufLen);
        newNode = calloc(1,sizeof(RESPONSES));
        if (ipAux == NULL || bufAux == NULL || newNode == NULL)
                return FAILURE;

        memcpy(ipAux,newRes->ip,newRes->ipLen);
        memcpy(bufAux,newRes->buf,newRes->bufLen);
        newNode->ip = ipAux;
        newNode->ipLen = newRes->ipLen;
        newNode->buf = bufAux;
        newNode->bufLen = newRes->bufLen;
        newNode->id = newRes->id;
        newNode->cBit = newRes->cBit;
        newNode->anCount = newRes->anCount;
        newNode->fromIface = newRes->fromIface;
        newNode->next = NULL;
        current->next = newNode;
        return SUCCESS;
}

PUBLIC void deleteResponsesList(RESPONSES **list)
{
        RESPONSES *current, *dispose;

        if (list == NULL || *list == NULL)
                return;
        current = *list;
        while (current != NULL) {
                dispose = current;
                current = current->next;
                if (dispose->buf != NULL)
                        free(dispose->buf);
                if (dispose->ip != NULL)
                        free(dispose->ip);
                free(dispose);
        }
        *list = NULL;
}

PUBLIC void deleteResponsesFrom(int ifIndex, RESPONSES *list)
{
        RESPONSES *current, *previous, *dispose;

        if (list == NULL)
                return;
        current = list;
        previous = current;
        while (current != NULL) {
                if (current->id == 0) {
                        previous = current;
                        current = current->next;
                        continue;
                }
                if (current->fromIface != ifIndex) {
                        previous = current;
                        current = current->next;
                        continue;
                }
                dispose = current;
                current = current->next;
                previous->next = current;
                if (dispose->buf != NULL)
                        free(dispose->buf);
                if (dispose->ip != NULL)
                        free(dispose->ip);
                free(dispose);
        }
}

PUBLIC int countResponses(RESPONSES *list)
{
        int count;
        RESPONSES *current;

        if (list == NULL)
                return 0;
        count = 0;
        current = list;
        for (; current != NULL; current = current->next) {
                if (current->id == 0)
                        continue;
                count++;
        }
        return count;
}

PUBLIC U_CHAR haveConflict(RESPONSES *list, int ifIndex)
{
    /*
     * Checks if two or more response were received from
     * differente respondes (checked by 'id' and 'CONFLICT' bit)
     */
        RESPONSES *current;

        if (list == NULL)
                return FALSE;
        current = list;
        if (countResponses2(list,ifIndex) < 2)
                return FALSE;
        for (; current != NULL; current = current->next) {
                if (current->id == 0)
                        continue;
                if (current->fromIface != ifIndex)
                        continue;
                if (current->cBit == 0)
                        return TRUE;
        }
        return FALSE;
}

PUBLIC U_CHAR haveValidResponse(RESPONSES *list, int ifIndex)
{
    /*
     * Checks if a valid response was received. i.e is not an 'SOA'
     * response (resource record not does not exists)
     */
        RESPONSES *current;

        if (list == NULL)
                return FALSE;
        current = list;
        if (countResponses2(list,ifIndex) < 1) {
                return FALSE;
        }
        for (; current != NULL; current = current->next) {
                if (current->id == 0)
                        continue;
                if (current->fromIface != ifIndex)
                        continue;
                if (current->anCount >= 1)
                        return TRUE;
        }
        return FALSE;
}

PUBLIC void printResponses(RESPONSES *list)
{
        RESPONSES *current;

        if (list == NULL)
                return;
        current = list;
        for (; current != NULL; current = current->next) {
                if (current->id == 0) {
                        printToStream("Response-head\n");
                        continue;
                }
                printToStream("C: %d. Id: %d. Ancount: %d. fromIf: %d\n",
                              current->cBit,current->id,current->anCount,
                              current->fromIface);
                printToStream("Ip: ");
                printBytes(current->ip,current->ipLen);
                printToStream("Buf: ");
                printBytes(current->buf,current->bufLen);
                printToStream("----------------------------------\n");
        }
        printToStream("Nro respuestas: %d\n",countResponses(list));
        printToStream("----------------------------------\n");
}

PUBLIC void printResponse(RESPONSES *response)
{
        printToStream("C: %d. Id: %d. Ancount: %d. fromIf: %d\n",
                      response->cBit,response->id,response->anCount,
                      response->fromIface);
        printToStream("Ip: ");
        printBytes(response->ip,response->ipLen);
        printToStream("Buf: ");
        printBytes(response->buf,response->bufLen);
}

PRIVATE int countResponses2(RESPONSES *list, int ifIndex)
{
        int count;
        RESPONSES *current;

        if (list == NULL)
                return 0;
        count = 0;
        current = list;
        for (; current != NULL; current = current->next) {
                if (current->id == 0)
                        continue;
                if (current->fromIface != ifIndex)
                        continue;
                count++;
        }
        return count;
}
