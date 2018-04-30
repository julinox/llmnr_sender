/*
 * Module for read and write the LLMNR cache file
 * Answers are stored into the next header format (By = bytes):
 *
 * |----8By----|---2By---|--2By--|--2By--|---2By----|-2By-|-------|
 * |expire-time|name size| QTYPE |ANCOUNT|payloadLen| TTL |PAYLOAD|
 *
 * Payload has the next format:
 *
 * |----2By---|---RDLENGTH By---|
 * | RDLENGTH |     RDATA       |
 */

#ifndef NSS_LLMNR_CACHE_H
#define NSS_LLMNR_CACHE_H
#include "../include/nss_llmnr_defs.h"

typedef struct {
        time_t expireAt;
        U_SHORT nameSz;
        U_SHORT type;
        U_SHORT anCount;
        U_SHORT payloadLen;
        int ttl;
        const char *name;
        U_CHAR *buffer;
} CACHEANSWER;

PUBLIC void cacheAnswers(const char *name, U_SHORT type,ANSWERS *answers);
PUBLIC int getCachedAnswer(const char *name, U_SHORT type,ANSWERS *answers);
#endif
