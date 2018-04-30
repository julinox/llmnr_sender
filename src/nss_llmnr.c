/* Macros */
#define BUFSZ 256

/* Includes */
#include <nss.h>
#include <poll.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

/* Own includes */
#include "../include/nss_llmnr_utils.h"
#include "../include/nss_llmnr_netinterface.h"
#include "../include/nss_llmnr_answers.h"
#include "../include/nss_llmnr_cache.h"
#include "../include/nss_llmnr_s1.h"
#include "../include/nss_llmnr_s2.h"
#include "../include/nss_llmnr_s3.h"
#include "../include/nss_llmnr_cacheclean.h"
#include "../include/nss_llmnr.h"

/* Enums & Structs */

/* Glocal variables */

/* Private prototypes */
PRIVATE int _main(const ARGS *args);

/* Functions definitions */
enum nss_status _nss_llmnr_gethostbyname_r(const char *name,
                                           struct hostent *result_buf,
                                           char *buf, size_t buflen,
                                           int *errnop, int *h_errnop)
{
    /*
     * Group all the parameters into a 'ARGS' struct, build the dns name
     * then call '_main()'
     */
        int status;
        ARGS *args;
        char *dName;

        args = calloc(1,sizeof(ARGS));
        if (args == NULL) {
                *errnop = errno;
                *h_errnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        if (name == NULL)
                return NSS_STATUS_NOTFOUND;

        dName = calloc(1,strlen(name) + 1 + 3);
        if (dName == NULL) {
                free(args);
                *errnop = errno;
                *h_errnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }

        strToDnsStr(name,dName);
        args->name = dName;
        args->cName = name;
        args->resultBuf = result_buf;
        args->buf = buf;
        args->bufLen = buflen;
        args->errnop = errnop;
        args->herrnop = h_errnop;
        args->type = A;
        args->addr = NULL;
        args->len = 0;
        args->format = 0;
        status = _main(args);
        free(dName);
        free(args);
        return status;
}

enum nss_status _nss_llmnr_gethostbyname2_r (const char *name, int af,
                                             struct hostent *result_buf,
                                             char *buf, size_t buflen,
                                             int *errnop, int *h_errnop)
{
    /*
     * Group all the parameters into a 'ARGS' struct, build dns name
     * then call '_main()'
     */
        int status;
        ARGS *args;
        U_SHORT type;
        char *dName;

        args = calloc(1,sizeof(ARGS));
        if (args == NULL) {
                *errnop = errno;
                *h_errnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        if (name == NULL)
                return NSS_STATUS_NOTFOUND;

        dName = calloc(1,strlen(name) + 1 + 3);
        if (dName == NULL) {
                free(args);
                *errnop = errno;
                *h_errnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }

        type = NONE;
        if (af == AF_INET)
                type = A;
        else if (af == AF_INET6)
                type = AAAA;

        strToDnsStr(name,dName);
        args->name = dName;
        args->cName = name;
        args->resultBuf = result_buf;
        args->buf = buf;
        args->bufLen = buflen;
        args->errnop = errnop;
        args->herrnop = h_errnop;
        args->type = type;
        args->addr = NULL;
        args->len = 0;
        args->format = 0;
        status = _main(args);
        free(dName);
        free(args);
        return status;
}

enum nss_status _nss_llmnr_gethostbyaddr_r(const void *addr, socklen_t len,
                                           int format,
                                           struct hostent *result_buf,
                                           char *buf, size_t buflen,
                                           int *errnop, int *h_errnop)
{
    /*
     * Group all the parameters into a 'ARGS' struct, build
     * the 'PTR' name associated to 'addr',build the dns name,
     * then call '_main()'
     */
        int status;
        ARGS *args;
        char *ptrName, *dName;

        if (addr == NULL)
                return NSS_STATUS_NOTFOUND;
        args = NULL;
        ptrName = NULL;

        args = calloc(1,sizeof(ARGS));
        if (args == NULL) {
                *errnop = errno;
                *h_errnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        ptrName = calloc(1,HOSTNAMEMAX + 1);
        if (ptrName == NULL) {
                *errnop = errno;
                *h_errnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        if (format == AF_INET && len == sizeof(struct in_addr))
                strToPtrStr(addr,AF_INET,ptrName);
        else if (format == AF_INET6 && len == sizeof(struct in6_addr))
                strToPtrStr(addr,AF_INET6,ptrName);
        else {
                free(ptrName);
                free(args);
                return NSS_STATUS_NOTFOUND;
        }

        dName = calloc(1,strlen(ptrName) + 1 + 3);
        if (dName == NULL) {
                free(ptrName);
                free(args);
                *errnop = errno;
                *h_errnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        strToDnsStr(ptrName,dName);
        args->name = dName;
        args->cName = ptrName;
        args->resultBuf = result_buf;
        args->buf = buf;
        args->bufLen = buflen;
        args->errnop = errnop;
        args->herrnop = h_errnop;
        args->type = PTR;
        args->addr = addr;
        args->len = len;
        args->format = format;
        status = _main(args);
        free(ptrName);
        free(dName);
        free(args);
        return status;
}

PRIVATE int _main(const ARGS *args)
{
    /*
     * - Look for answer into the cache
     * - If answer not cached, then send LLMNR query
     * - 'answers' will contain the answers received (if any)
     * - Fill the hostent struct with the 'answers' received
     */
        NETIFACE *ifaces;
        ANSWERS *answers;
        int status, clean;

        clean = 0;
        if (args->name == NULL ||args->buf == NULL ||
            args->resultBuf == NULL || args->errnop == NULL ||
            args->herrnop == NULL)
                return NSS_STATUS_NOTFOUND;

        status = NSS_STATUS_NOTFOUND;
        *args->errnop = 0;
        *args->herrnop = 0;
        ifaces = netIfListHead();
        answers = answersListhead();

        if (ifaces == NULL|| answers == NULL) {
                *args->errnop = errno;
                *args->herrnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        clean = getCachedAnswer(args->cName,args->type,answers);
        if (countAnswersByType(args->type,answers) > 0) {
                status = isValidAnswer(args->type,answers);
        } else {

                status = startS1(args,ifaces);
                if (status == NSS_STATUS_SUCCESS)
                        status = startS2(args,ifaces,answers);
        }
        if (status == NSS_STATUS_SUCCESS)
                status = startS3(args,answers);
        delNetIfList(&ifaces);
        deleteAnswersList(&answers);
        if (clean)
                cleanCache();
        return status;
}
