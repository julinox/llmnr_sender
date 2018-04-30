/* Macros */
#define LABELSZ 63

/* Includes */
#include <nss.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* Own includes */
#include "../include/nss_llmnr_print.h"
#include "../include/nss_llmnr_strlist.h"
#include "../include/nss_llmnr_utils.h"
#include "../include/nss_llmnr_answers.h"

/* Private prototypes */

/* Glocal variables */

/* Function definitions */
PUBLIC ANSWERS *answersListhead()
{
    /*
     * Creates a 'ANSWERS' list head
     */
        ANSWERS *head;

        head = calloc(1,sizeof(ANSWERS));
        if (head == NULL)
                return NULL;
        head->TYPE = NONE;
        head->TTL = 0;
        head->RDLENGTH = 0;
        head->RDATA = NULL;
        head->next = NULL;
        return head;
}

PUBLIC int newAnswer(ANSWERS *list, U_SHORT type, int ttl, void *data,
                     unsigned short dLen)
{
    /*
     * Adds a new 'ANSWERS' to the list
     */
        ANSWERS *aux, *newNode;

        if (list == NULL || data == NULL)
                return SUCCESS;
        newNode = calloc(1,sizeof(ANSWERS));
        if (newNode == NULL)
                return FAILURE;
        aux = list;
        while (aux->next != NULL)
                aux = aux->next;
        switch (type) {
        case A:
                newNode->RDATA = calloc(1,dLen);
                if (newNode->RDATA == NULL) {
                        free(newNode);
                        return FAILURE;
                }
                newNode->RDLENGTH = dLen;
                memcpy(newNode->RDATA,data,dLen);
                break;
        case AAAA:
                newNode->RDATA = calloc(1,dLen);
                if (newNode->RDATA == NULL) {
                        free(newNode);
                        return FAILURE;
                }
                newNode->RDLENGTH = dLen;
                memcpy(newNode->RDATA,data,dLen);
                break;
        case PTR:
                newNode->RDATA = calloc(1,strlen(data) + 1);
                if (newNode->RDATA == NULL) {
                        free(newNode);
                        return FAILURE;
                }
                newNode->RDLENGTH = strlen(data) + 1;
                strcpy(newNode->RDATA,data);
                break;
        default:
                free(newNode);
                return FAILURE;
        }
        newNode->TYPE = type;
        newNode->TTL = ttl;
        newNode->next = NULL;
        aux->next = newNode;
        return SUCCESS;
}

PUBLIC int countAnswers(ANSWERS *list)
{
        int count;
        ANSWERS *aux;

        if (list == NULL)
                return 0;
        count = 0;
        aux = list;
        for (; aux != NULL; aux = aux->next) {
                if (aux->TYPE == NONE)
                        continue;
                count++;
        }
        return count;
}

PUBLIC int countAnswersByType(U_SHORT type, ANSWERS *list)
{
    /*
     * Count the number of ANSWERSs of type 'type'
     */
        int count;
        ANSWERS *aux;

        if (list == NULL)
                return 0;
        count = 0;
        aux = list;
        for (; aux != NULL; aux = aux->next) {
                if (aux->TYPE == type)
                        count ++;
        }
        return count;
}

PUBLIC int countAnswersBytes(U_SHORT type, ANSWERS *list)
{
    /*
     * Like 'countRespByType' but this one sums bytes
     * per ANSWERS
     */
        int count;
        ANSWERS *aux;

        if (list == NULL)
                return 0;
        count = 0;
        aux = list;
        for (; aux != NULL; aux = aux->next) {
                if (aux->TYPE == type)
                        count += aux->RDLENGTH;
        }
        return count;
}

PUBLIC void deleteAnswersList(ANSWERS **list)
{
        ANSWERS *aux, *dispose;

        if (*list == NULL)
                return;
        aux = *list;
        while (aux->next != NULL) {
                dispose = aux;
                aux = aux->next;
                free(dispose->RDATA);
                free(dispose);
        }
        free(aux->RDATA);
        free(aux);
        *list = NULL;
}

PUBLIC int getAnswerTTL(ANSWERS *list)
{
        if (list == NULL || list->next == NULL)
                return 0;
        return list->next->TTL;
}

PUBLIC int isValidAnswer(U_SHORT type, ANSWERS *list)
{
    /*
     * Check if a list of 'ANSWERS' is good enough. i.e its
     * not a 'SOA' answer (negative caching)
     */
        ANSWERS *current;

        if (list == NULL)
                return NSS_STATUS_NOTFOUND;
        if (countAnswersByType(type,list) < 1)
                return NSS_STATUS_NOTFOUND;
        current = list;
        for (; current != NULL; current = current->next) {
                if (current->TYPE == NONE)
                        continue;
                if (current->RDLENGTH == 0)
                        return NSS_STATUS_NOTFOUND;
        }
        return NSS_STATUS_SUCCESS;
}

PUBLIC void printAnswers(ANSWERS *list)
{
        ANSWERS *aux;

        if (list == NULL)
                return;
        aux = list;
        for (; aux != NULL; aux = aux->next) {
                if (aux->TYPE == NONE)
                        continue;
                switch (aux->TYPE) {
                case A:
                        printToStream("IPv4 %d %d ",aux->RDLENGTH,
                                      aux->TTL);
                        printBytes(aux->RDATA,aux->RDLENGTH);
                        break;
                case AAAA:
                        printToStream("IPv6 %d %d ",aux->RDLENGTH,
                                      aux->TTL);
                        printBytes(aux->RDATA,aux->RDLENGTH);
                        break;
                case PTR:
                        printToStream("PTR %d %d ",aux->RDLENGTH,
                                      aux->TTL);
                        printToStream("%s\n",aux->RDATA);
                        break;
                case SOA:
                        printToStream("IPv6 %d %d ",aux->RDLENGTH,
                                      aux->TTL);
                        printBytes(aux->RDATA,aux->RDLENGTH);
                default:
                        printToStream("Type: %d. TTL: %d. RdLen: %d\n",aux->TYPE,
                                      aux->TTL,aux->RDLENGTH);
                        printBytes(aux->RDATA,aux->RDLENGTH);
                }
        }
        printToStream("Answers: %d\n",countAnswers(list));
}
