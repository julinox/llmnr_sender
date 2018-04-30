/** ****************************************************
 * The LLMNR cache cleaner                             *
 * Iterate through the LLMNR cache file and save       *
 * all the still valid responses cached.               *
 * Note: fork() is needed since this program is called *
 * using the syscall 'system()'                        *
 *******************************************************/

/* Macros */

/* Includes */
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>


/* Own includes */
#include "../include/nss_llmnr_defs.h"
#include "../include/nss_llmnr_cacheclean.h"

/* Enums & Structs */
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

/* Glocal variables */
PRIVATE char CacheFile[] = "/tmp/llmnr.cache";
PRIVATE char CacheFileAux[] = "/tmp/llmnr.cache.aux";

/* Private prototypes */
PRIVATE void releaseLock(FILE *f);
PRIVATE int checkMode(char *mode);
PRIVATE int readAndWrite(FILE *oldFile, FILE *newFile);
PRIVATE FILE *getLockNb(char *pathName, char *mode);
//PRIVATE FILE *getLockB(char *pathName, char *mode);

/* Functions definitions */
PUBLIC void cleanCache()
{
    /*
     * - Get lock from the cache file
     * - Get lock from an auxiliary file
     * - Read from cache file and write into
     *   auxiliary file
     * - Rename file
     * - Release locks
     */
        int status;
        FILE *oldFile, *newFile;

        oldFile = getLockNb(CacheFile,"rb");
        if (oldFile == NULL)
                return;
        newFile = getLockNb(CacheFileAux,"wb");
        if (newFile == NULL) {
                releaseLock(oldFile);
                return;
        }
        status = readAndWrite(oldFile,newFile);
        if (status) {
                remove(CacheFile);
                remove(CacheFileAux);
        }
        releaseLock(oldFile);
        releaseLock(newFile);
}

PRIVATE int readAndWrite(FILE *oldFile, FILE *newFile)
{
    /*
     * Read a response cached and checks if still is
     * a valid response (i.e TTL has not expired yet)
     * If so, then write it to a new file and when done
     * reading and writing rename the file
     */
        char flag;
        int offset;
        U_SHORT uSz;
        U_CHAR *buffer;
        CACHEANSWER cache;
        time_t currentTime;

        if (oldFile == NULL || newFile == NULL)
                return SUCCESS;
        buffer = calloc(1,TCPBUFSZ);
        if (buffer == NULL)
                return SUCCESS;

        flag = 0;
        offset = 0;
        uSz = sizeof(U_SHORT);
        memset(&cache,0,sizeof(CACHEANSWER));
        currentTime = time(NULL);
        if (currentTime == (time_t)-1) {
                free(buffer);
                return SUCCESS;
        }

        while (TRUE) {
                fread(buffer,CHEADZ,1,oldFile);
                if (feof(oldFile))
                        break;
                if (ferror(oldFile)) {
                        flag = 1;
                        break;
                }
                offset = 0;
                cache.expireAt = *((time_t *)(buffer + offset));
                offset += sizeof(time_t);
                cache.nameSz = *((U_SHORT *)(buffer + offset));
                offset += (3 * uSz);
                cache.payloadLen = *((U_SHORT *)(buffer + offset));

                fread(buffer + CHEADZ,cache.nameSz + cache.payloadLen,
                      1,oldFile);
                if (feof(oldFile))
                        break;
                if (ferror(oldFile)) {
                        flag = 1;
                        break;
                }
                if (cache.expireAt > currentTime) {
                        if (!fwrite(buffer,CHEADZ + cache.nameSz +
                                    cache.payloadLen,1,newFile)) {
                                flag = 1;
                        }
                }
        }
        if (flag || rename(CacheFileAux,CacheFile)) {
                free(buffer);
                return FAILURE;
        }
        free(buffer);
        return SUCCESS;
}

PRIVATE FILE *getLockNb(char *pathName, char *mode)
{
    /*
     * Open cache file and get lock (non blocking)
     */
        FILE *f;
        int lock;

        if (pathName == NULL || mode == NULL)
                return NULL;
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

PRIVATE void releaseLock(FILE *f)
{
        if (f == NULL)
                return;
        flock(fileno(f),LOCK_UN);
        if (f != NULL)
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
        else if (strcasecmp(mode,"w+"))
                return TRUE;
        return FALSE;
}

/*
PRIVATE FILE *getLockB(char *pathName, char *mode)
{
    //Open cache file and get lock (blocking)
        FILE *f;
        int lock;

        if (pathName == NULL || mode == NULL)
                return NULL;
        if (!checkMode(mode))
                return NULL;
        f = fopen(pathName,mode);
        if (f == NULL)
                return NULL;
        lock = LOCK_EX;
        if (flock(fileno(f),lock)) {
                fclose(f);
                return NULL;
        }
        return f;
}
*/
