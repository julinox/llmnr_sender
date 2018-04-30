/* Macros */

/* Includes */
#include <nss.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>

/* Own includes */
#include "../include/nss_llmnr_utils.h"
#include "../include/nss_llmnr_answers.h"
#include "../include/nss_llmnr_responses.h"
#include "../include/nss_llmnr_s3.h"

/* Enums & Structs */

/* Glocal variables */

/* Private prototypes */
PRIVATE int __start(const ARGS *args, ANSWERS *answers);
PRIVATE int fill46Hostent(const ARGS *args, ANSWERS *list);
PRIVATE int fillPtrHostent(const ARGS *args, ANSWERS *list);

/* Functions definitions */
PUBLIC int startS3(const ARGS *args, ANSWERS *answers)
{
        int status;

        status = NSS_STATUS_NOTFOUND;
        if (args == NULL || answers == NULL)
                return NSS_STATUS_NOTFOUND;
        status = __start(args,answers);
        return status;
}

PRIVATE int __start(const ARGS *args, ANSWERS *list)
{
    /*
     * Fill 'buf' and 'result_buf' with the responses contained
     * into 'list' (Only 'A', 'AAAA'and 'PTR')
     */
        int status;

        memset(args->buf,0,args->bufLen);
        memset(args->resultBuf,0,sizeof(_HOST));
        status = NSS_STATUS_NOTFOUND;
        switch (args->type) {
        case A:
                status = fill46Hostent(args,list);
                break;
        case AAAA:
                status = fill46Hostent(args,list);
                break;
        case PTR:
                status = fillPtrHostent(args,list);
                break;
        default:
                status = NSS_STATUS_NOTFOUND;
        }
        return status;
}

PRIVATE int fill46Hostent(const ARGS *args, ANSWERS *list)
{
    /*
     * Fill the hostent struct associated with a 'A' or 'AAAA'
     * query response. Assuming that 'list' contain 2 responses
     * for a given address, the layout of 'buf' is the following:
     *
     *               |--------h_addr_list[]---------|----addrs----|
     * ----------------------------------------------------------
     * |name |NullPtr|ptrToAddr1|ptrToAddrr2|NullPtr|addr1 |addr2 |
     * ------------------------------------------------------------
     * |----------------------------buf---------------------------|
     *
     * Note: Pointer to "name" will be stored into 'h_name'
     */
        int i;
        char *ptrCpy;
        ANSWERS *aux;
        size_t szNeeded, resps, respsBytes;

        if (args->name == NULL) {
                *args->errnop = EAGAIN;
                *args->herrnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }

        i = 0;
        aux = list;
        resps = countAnswersByType(args->type,list);
        respsBytes = countAnswersBytes(args->type,list);
        szNeeded = strlen(args->name) + 1 + sizeof(char*) +
                   ((resps + 1) * sizeof(char*)) + respsBytes + resps;
        if (args->bufLen <= szNeeded) {
                *args->errnop = ERANGE;
                *args->herrnop = ERANGE;
                return NSS_STATUS_TRYAGAIN;
        }
        args->resultBuf->h_name = args->buf;
        args->resultBuf->h_aliases = NULL;
        args->resultBuf->h_addr_list = (char **) (args->buf +
                                                  strlen(args->name) + 1 +
                                                  sizeof(char*));
        ptrCpy = args->buf + strlen(args->name) + 1 + sizeof(char*) +
                 ((resps + 1) * sizeof(char*));

        strcpy(args->buf,args->cName);
        switch (args->type) {
        case A:
                args->resultBuf->h_addrtype = AF_INET;
                args->resultBuf->h_length = IPV4LEN;
                break;
        case AAAA:
                args->resultBuf->h_addrtype = AF_INET6;
                args->resultBuf->h_length = IPV6LEN;
                break;
        }
        for (; aux != NULL; aux = aux->next) {
                if (aux->TYPE != args->type)
                        continue;
                memcpy(ptrCpy,aux->RDATA,aux->RDLENGTH);
                args->resultBuf->h_addr_list[i] = ptrCpy;
                ptrCpy += aux->RDLENGTH + 1;
                i++;

        }
        return NSS_STATUS_SUCCESS;
}

PRIVATE int fillPtrHostent(const ARGS *args, ANSWERS *list)
{
    /*
     * Fill the hostent struct associated with a 'PTR' query
     * response. Assuming that 'list' contain 2 responses for
     * a given address, the layout of 'buf' is the following:
     *
     *       |--h_addr_list[]--|---h_aliases[]----|----names----|
     * ----------------------------------------------------------
     * |addr |ptrToAddr|NullPtr|ptrToName2|NullPtr|Name1 |Name2 |
     * ----------------------------------------------------------
     * |---------------------------buf--------------------------|
     *
     * Note: The pointer to the first "Name1" will be stored
     * into 'h_name'
     */
        int i;
        char *ptrCpy;
        ANSWERS *aux;
        char nameAux[HOSTNAMEMAX + 1];
        size_t szNeeded, resps, respsBytes;

        i = -1;
        aux = list;
        resps = countAnswersByType(PTR,list);
        respsBytes = countAnswersBytes(PTR,list);
        szNeeded = args->len + 1 + (2 * sizeof(char*)) +
                   (resps  * sizeof(char*)) + respsBytes;
        if (args->bufLen <= szNeeded) {
                *args->errnop = ERANGE;
                *args->herrnop = ERANGE;
                return NSS_STATUS_TRYAGAIN;
        }
        args->resultBuf->h_name = args->buf + args->len + 1 +
                                  (2 * sizeof(char*)) +
                                  (resps * sizeof(char*));
        args->resultBuf->h_aliases = (char **) (args->buf + args->len + 1 +
                                                (2 * sizeof(char*)));
        args->resultBuf->h_addr_list = (char **) (args->buf + args->len + 1);
        args->resultBuf->h_addr_list[0] = args->buf;
        args->resultBuf->h_addr_list[1] = NULL;
        ptrCpy = args->resultBuf->h_name;

        if (args->addr != NULL)
                memcpy(args->buf,args->addr,args->len);
        args->resultBuf->h_addrtype = args->format;
        args->resultBuf->h_length = args->len;
        for (; aux != NULL; aux = aux->next) {
                if (aux->TYPE != args->type)
                        continue;
                memset(nameAux,0,HOSTNAMEMAX + 1);
                if (dnsStrToStr(aux->RDATA,nameAux) != NULL)
                        strcpy(ptrCpy,nameAux);
                else
                        memcpy(ptrCpy,aux->RDATA,aux->RDLENGTH);
                if (i >= 0)
                        args->resultBuf->h_aliases[i] = ptrCpy;
                ptrCpy += aux->RDLENGTH;
                i++;
        }
        return NSS_STATUS_SUCCESS;
}
