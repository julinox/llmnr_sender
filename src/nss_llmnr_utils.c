/* Macros */
#define LABELSZ 63

/* Includes */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/nss_llmnr_strlist.h"
#include "../include/nss_llmnr_utils.h"

/* Private prototypes */
PRIVATE void hexToAsciiPtr(U_CHAR byte, char *ret);
PRIVATE U_CHAR hexToAsciiPtrH(U_CHAR nibble);

/* Glocal variables */

/* Function definitions */
PUBLIC int checkFQDN(char *name)
{
    /*
     * Checks if 'name' is a valid fully qualified
     * domain name
     */
        int i, len;
        STRNODE *sp, *aux;

        if (name == NULL)
                return SUCCESS;
        len = strlen(name);
        if (len > HOSTNAMEMAX)
                return FAILURE;
        for (i = 0; i < len; i++) {
                if (!isalnum(name[i]) && name[i] != '-'
                    && name[i] != '.')
                return FAILURE;        }

        if (name[0] == '-' || name[len - 1] == '-')
                return FAILURE;
        if (name[0] == '.' && len > 1)
                return FAILURE;
        for (i = 0; i < len; i++) {
                if (name[i] == '.' || name[i] == '-') {
                        if (name[i + 1] == '.' || name[i + 1] == '-')
                                return FAILURE;
                }
        }
        sp = split(name,'.');
        aux = sp;
        while (aux != NULL) {
                if (aux->string != NULL) {
                        if (strlen(aux->string) > LABELSZ)
                                return FAILURE;
                }
                aux = aux->next;
        }
        deleteStrNodeList(&sp);
        return SUCCESS;
}

PUBLIC char *strToDnsStr(const char *name, char *buff)
{
    /*
     * Regular string to DNS string format converter
     * Undefined behavior when strlen(name) > buffSz
     */
        unsigned int head, tail, count, stringSize;

        if (name == NULL)
                return NULL;
        tail = 0;
        head = 1;
        count = 0;
        stringSize = strlen(name) + 1;
        if (stringSize <= 1) {
                *buff = 0x1;
                buff[1] = *name;
                return buff;
        }
        buff[0] = '.';
        strcpy(buff + 1,name);
        while (head < stringSize) {
                if (buff[head] == '.') {
                        buff[tail] = count;
                        tail = head;
                        count = 0;
                } else {
                        count++;
                }
                head++;
        }
        if (tail == 0)
                buff[0] = count;
        else
                buff[tail] = count;
        return buff;
}

PUBLIC char *dnsStrToStr(char *name, char *buff)
{
    /*
     * DNS string format to regular string converter
     */
        U_CHAR i;
        char *ptr, *ptrB;
        if (name == NULL || buff == NULL)
                return NULL;
        ptr = name;
        ptrB = buff;
        i = *ptr++;
        while (*ptr) {
                if (i <= 0) {
                        *ptrB = '.';
                        ptrB++;
                        i = *ptr++;
                }
                *ptrB = *ptr;
                ptrB++;
                ptr++;
                i--;
        }
        return buff;
}

PUBLIC void strToPtrStr(const void *ip, int type, char *buff)
{
    /*
     * Given an ip of type 'type' (IPv4 or IPv6) build his
     * corresponding PTR name (See RFC 1035) and store it
     * into 'buff'. It assumes that buff is at least of size
     * 'HOSTNAMEMAX + 1'. 'ip' is in network byte order
     */
        int i;
        char str[16];
        U_CHAR *ptr;
        char *aux, auxStr[75];
        char newName[HOSTNAMEMAX + 1];
        char domain4[] = "in-addr.arpa.";
        char domain6[] = "ip6.arpa.";

        memset(auxStr,0,75);
        memset(newName,0,HOSTNAMEMAX + 1);
        ptr = (U_CHAR *)ip;

        switch (type) {
        case AF_INET:
                for (i = IPV4LEN - 1; i >= 0; i--) {
                        memset(str,0,16);
                        sprintf(str, "%d", (ptr[i]));
                        strcat(auxStr,str);
                        strcat(auxStr,".");
                }
                strcpy(newName,auxStr);
                strcat(newName,domain4);
                break;
        case AF_INET6:
                aux = auxStr;
                for (i = IPV6LEN - 1; i >=0; i--) {
                        hexToAsciiPtr(ptr[i],aux);
                        aux[1] = '.';
                        aux[3] = '.';
                        aux += 4;
                }
                strcpy(newName,auxStr);
                strcat(newName,domain6);
                break;
        }
        strcpy(buff,newName);
}

PRIVATE void hexToAsciiPtr(U_CHAR byte, char *ret)
{
    /*
     * Special hex to ascii (PTR name only)
     * Given a byte will halve it, exchange it and turn it
     * into two chars separeted by a "."
     * Example: 0xAB -> "B.A"
     */
       U_CHAR highNibble, lowNibble;

        highNibble = byte >> 4;
        lowNibble = byte & 0x0F;
        ret[0] = hexToAsciiPtrH(lowNibble);
        ret[2] = hexToAsciiPtrH(highNibble);
}

PRIVATE U_CHAR hexToAsciiPtrH(U_CHAR nibble)
{
    /*
     * Helper function for 'hexToAsciiPtr'
     * Given a hex number will turn it into ascii character
     */
        U_CHAR ret;

        switch (nibble) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
                ret = nibble + 48;
                break;
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
                ret = nibble + 87;
                break;
        }
        return ret;
}
