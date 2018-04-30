/* Macros */
#define CLEANCACHE 150

/* Includes */
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/file.h>

/* Own includes */
#include "../include/nss_llmnr_utils.h"
#include "../include/nss_llmnr_answers.h"
#include "../include/nss_llmnr_cache.h"

/* Enums & Structs */

/* Glocal variables */
PRIVATE char CacheFile[] = "/tmp/llmnr.cache";

/* Private prototypes */
PRIVATE void _cacheAnswers(CACHEANSWER *cache, ANSWERS *answers);
PRIVATE void _getCachedAnswer(CACHEANSWER *cache, ANSWERS *answers);
PRIVATE FILE *oPenAndGetLock(char *pathName, char *mode);
PRIVATE void releaseFileLock(FILE *f);
PRIVATE int checkMode(char *mode);

/* Functions definitions */
PUBLIC void cacheAnswers(const char *name, U_SHORT type, ANSWERS *answers)
{
    /*
     * - Get lock from LLMNR cache file. If lock not
     *   acquired then the answer is not saved
     * - Save 'ANSWERS' list into LLMNR cache file
     */
        FILE *f;
        U_SHORT bufLen;
        CACHEANSWER cache;
        U_CHAR *buf, flag;

        if (answers == NULL)
                return;
        if (countAnswers(answers) < 1)
                return;
        flag = 0;
        f = NULL;
        memset(&cache,0,sizeof(CACHEANSWER));
        cache.nameSz = strlen(name) + 1;
        cache.anCount = countAnswersByType(type,answers);
        cache.payloadLen = countAnswersBytes(type,answers) +
                           cache.anCount * 2;
        bufLen = CHEADZ + cache.nameSz + cache.payloadLen;
        buf = calloc(1,bufLen);
        if (buf == NULL)
                return;

        cache.expireAt = 0;
        cache.type = type;
        cache.ttl = getAnswerTTL(answers);
        cache.name = name;
        cache.buffer = buf;
        _cacheAnswers(&cache,answers);
        f = oPenAndGetLock(CacheFile,"ab");
        if (f == NULL) {
                free(buf);
                return;
        }
        cache.expireAt = time(NULL);
        if (cache.expireAt == (time_t)-1)
                goto CLEANUP;
        cache.expireAt += getAnswerTTL(answers) + 1;
        memcpy(buf,&cache.expireAt,sizeof(time_t));
        if (!fwrite(buf,bufLen,1,f))
                flag = 1;

        CLEANUP:
        free(buf);
        releaseFileLock(f);
        if (flag)
                remove(CacheFile);
}

PUBLIC int getCachedAnswer(const char *name, U_SHORT type,ANSWERS *answers)
{
    /*
     * - Get lock from LLMNR cache file. If lock is not
     *   obtained the the cache search is dismissed
     * - Read 'ANSWERS' list saved into LLMNR cache file
     * - If answer found, parse it and add it to
     *   'ANSWERS' list
     * - If the number of cached answers checked is
     *   bigger than 'CLEANCACHE' the discard search
     *   and invoke the cache cleaner program
     */
        FILE *f;
        size_t strLen;
        int offset, ret;
        U_SHORT uSz, count;
        CACHEANSWER cache;
        U_CHAR *payloadBuf;
        U_CHAR head[CHEADZ];

        ret = 0;
        if (answers == NULL)
                return ret;
        f = oPenAndGetLock(CacheFile,"rb");
        if (f == NULL)
                return ret;
        payloadBuf = calloc(1,TCPBUFSZ);
        if (payloadBuf == NULL)
                goto CLEANUP2;

        count = 0;
        uSz = sizeof(U_SHORT);
        strLen = strlen(name) + 1;
        memset(head,0,CHEADZ);
        memset(&cache,0,sizeof(CACHEANSWER));

        while (TRUE) {
                fread(head,CHEADZ,1,f);
                if (feof(f))
                        break;
                if (ferror(f))
                        goto CLEANUP2;
                if (count > CLEANCACHE)
                        break;
                offset = 0;
                cache.expireAt = *((time_t *)(head + offset));
                offset += sizeof(time_t);
                cache.nameSz = *((U_SHORT *)(head + offset));
                offset += uSz;
                cache.type = *((U_SHORT *)(head + offset));
                offset += uSz;
                cache.anCount = *((U_SHORT *)(head + offset));
                offset += uSz;
                cache.payloadLen = *((U_SHORT *)(head + offset));
                offset += uSz;
                cache.ttl = *((int *)(head + offset));

                if (strLen != cache.nameSz || type != cache.type ||
                    time(NULL) > cache.expireAt) {
                        fseek(f,cache.nameSz + cache.payloadLen,SEEK_CUR);
                        count++;
                        continue;
                }

                fread(payloadBuf,cache.nameSz + cache.payloadLen,1,f);
                if (feof(f))
                        break;
                if (ferror(f))
                        goto CLEANUP2;
                cache.name = (char *) payloadBuf;
                cache.buffer = payloadBuf + cache.nameSz;
                if (!strcasecmp(name,cache.name)) {
                        _getCachedAnswer(&cache,answers);
                        break;
                }
                count++;
        }

        CLEANUP2:
        if (payloadBuf != NULL)
                free(payloadBuf);
        if (count > CLEANCACHE)
                ret = 1;
        releaseFileLock(f);
        return ret;
}

PRIVATE void _cacheAnswers(CACHEANSWER *cache, ANSWERS *answers)
{
    /*
     * Helper function for 'cacheAnswers()'
     */
        int offset;
        U_SHORT uSz;
        ANSWERS *current;

        current = answers;
        uSz = sizeof(U_SHORT);
        offset = sizeof(time_t);
        memcpy(cache->buffer + offset,&cache->nameSz,uSz);
        offset += uSz;
        memcpy(cache->buffer + offset,&cache->type,uSz);
        offset += uSz;
        memcpy(cache->buffer + offset,&cache->anCount,uSz);
        offset += uSz;
        memcpy(cache->buffer + offset,&cache->payloadLen,uSz);
        offset += uSz;
        memcpy(cache->buffer + offset,&cache->ttl,uSz);
        offset += sizeof(int);
        strcpy((char *) cache->buffer + offset, cache->name);
        offset += cache->nameSz;

        for (; current != NULL; current = current->next) {
                if (current->TYPE == NONE)
                        continue;
                if (current->TYPE != cache->type)
                        continue;
                memcpy(cache->buffer + offset,&current->RDLENGTH,uSz);
                offset += uSz;
                memcpy(cache->buffer + offset,current->RDATA,
                       current->RDLENGTH);
                offset += current->RDLENGTH;
        }
}


PRIVATE void _getCachedAnswer(CACHEANSWER *cache, ANSWERS *answers)
{
    /*
     * Helper function for 'getCachedAnswer()'
     */
        U_CHAR *data;
        U_SHORT count, uSz, dLen;

        count = 0;
        uSz = sizeof(U_SHORT);
        data = cache->buffer;

        for (; count < cache->anCount; count++) {
                dLen = *((U_SHORT *)data);
                data += uSz;
                newAnswer(answers,cache->type,cache->ttl,data,dLen);
                data += dLen;
        }
}

PRIVATE FILE *oPenAndGetLock(char *pathName, char *mode)
{
    /*
     * Open LLMNR cache file and get lock
     */
        FILE *f;
        int lock;

        if (!checkMode(mode))
                return NULL;
        f = fopen(pathName,mode);
        if (f == NULL)
                return NULL;
        lock = LOCK_EX | LOCK_NB;
        if (flock(fileno(f),lock)) {
                fclose(f);
                return NULL;
        }
        return f;
}

PRIVATE void releaseFileLock(FILE *f)
{
        if (f == NULL)
                return;
        flock(fileno(f),LOCK_UN);
        fclose(f);
}

PRIVATE int checkMode(char *mode)
{
        if (strcasecmp(mode,"r"))
                return TRUE;
        else if (strcasecmp(mode,"r+"))
                return TRUE;
        else if (strcasecmp(mode,"w"))
                return TRUE;
        else if (strcasecmp(mode,"w+"))
                return TRUE;
        else if (strcasecmp(mode,"a"))
                return TRUE;
        else if (strcasecmp(mode,"a+"))
                return TRUE;
        else if (strcasecmp(mode,"rb"))
                return TRUE;
        else if (strcasecmp(mode,"wb"))
                return TRUE;
        else if (strcasecmp(mode,"ab"))
                return TRUE;
        return FALSE;
}
