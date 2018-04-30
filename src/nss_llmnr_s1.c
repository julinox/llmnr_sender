/* Macros */
#define LINESZ 300
#define MAXLINES 40

/* Includes */
#include <nss.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/nss_llmnr_strlist.h"
#include "../include/nss_llmnr_netinterface.h"
#include "../include/nss_llmnr_s1.h"

/* Enums & Structs */

/* Glocal variables */

/* Private prototypes */
PRIVATE int __start(const ARGS *args, NETIFACE *ifaces);
PRIVATE void readConfigFile(char *filePath, NETIFACE *ifaces);
PRIVATE void parseConfigFile(STRNODE *params, NETIFACE *ifaces);
PRIVATE void _parseConfigFile(STRNODE *param, NETIFACE *ifaces);
PRIVATE void validIface(char *ifName, char *family, NETIFACE *ifaces);
PRIVATE void createInterfaces(struct ifaddrs *head, NETIFACE *ifaces);
PRIVATE void fillInterfaces(struct ifaddrs *head, NETIFACE *ifaces);
PRIVATE void _fillInterfaces(NETIFACE *copy, struct ifaddrs *aux);
PRIVATE void remLoopback(NETIFACE *ifaces);

/* Functions definitions */
PUBLIC int startS1(const ARGS *args, NETIFACE *ifaces)
{
        int status;

        status =__start(args,ifaces);
        return status;
}

PRIVATE int __start(const ARGS *args, NETIFACE *ifaces)
{
    /*
     * - Checks if llmnr.con file exists. If so then read
     *   it and take the interfaces able to send LLMNR queries
     * - Build the netiface list and fill the interfaces IPs
     */
        struct ifaddrs *head;
        char filePath[] = "/etc/llmnr/llmnr.conf";

        if (!access(filePath,F_OK)) {
                readConfigFile(filePath,ifaces);
        }
        if (getifaddrs(&head) < 0) {
                if (head != NULL)
                        freeifaddrs(head);
                *args->errnop = errno;
                *args->herrnop = EAGAIN;
                return NSS_STATUS_TRYAGAIN;
        }
        if (netIfaceListSz(ifaces) <= 1) {
                createInterfaces(head,ifaces);
                ifaces->flags |= _IFF_DYNAMIC;
        } else {
                ifaces->flags |= _IFF_STATIC;
        }
        fillInterfaces(head,ifaces);
        freeifaddrs(head);
        remLoopback(ifaces);
        return NSS_STATUS_SUCCESS;
}

PRIVATE void readConfigFile(char *filePath, NETIFACE *ifaces)
{
    /*
     * Get parameters from /etc/llmnr.conf and stores them
     * into a 'strList' list for later parsing
     */
        STRNODE *params;
        int i, fd, lineCount;
        char readChar, commentary, line[LINESZ];

        i = 0;
        lineCount = 0;
        commentary = 0;
        memset(line,0,LINESZ + 1);
        fd = open(filePath,O_RDONLY);

        if (fd < 0)
                return;
        params = strNodeHead();
        while (read(fd,&readChar,sizeof(char)) > 0 && lineCount < MAXLINES) {
                if (readChar == '#')
                        commentary = 1;
                if (readChar == 10) {
                        if (i > 0) {
                                if (newStrNode(line,params)) {
                                        deleteStrNodeList(&params);
                                        return;
                                }
                                lineCount++;
                        }
                        i = 0;
                        commentary = 0;
                        memset(line,0,LINESZ + 1);
                        continue;
                }
                if (!commentary && i < LINESZ)
                        line[i++] = readChar;
        }
        close(fd);
        parseConfigFile(params,ifaces);
        deleteStrNodeList(&params);
}

PRIVATE void parseConfigFile(STRNODE *params, NETIFACE *ifaces)
{
    /*
     * Parse the parameters took from config file
     */
        STRNODE *aux;
        int delimiter;

        delimiter = 32;
        for (; params != NULL; params = params->next) {
                if (params->string == NULL)
                        continue;
                aux = split(params->string,delimiter);
                if (aux == NULL) {
                        deleteStrNodeList(&params);
                        return;
                }
                _parseConfigFile(aux,ifaces);
                deleteStrNodeList(&aux);
        }
}

PRIVATE void _parseConfigFile(STRNODE *param, NETIFACE *ifaces)
{
    /*
     * parseConfigFile() helper
     * Given a string (containing a line read from config file)
     * checks for predefined patterns (hostname, iface, mx,etc)
     * If no pattern found then line its considered an invalid
     * parameter
     * If pattern found then check if the parameter is valid
     */
        char *key;
        int strNodeCount;

        key = getNodeAt(param,1);
        strNodeCount = countStrNodes(param);

        if (strNodeCount == 4 && !strcasecmp("iface",key))
                validIface(getNodeAt(param,2),getNodeAt(param,3),ifaces);
}

PRIVATE void validIface(char *ifName, char *family, NETIFACE *ifaces)
{
    /*
     * Checks if the given interface (read from config
     * file is valid. If valid then a new 'NETIFACE' node
     * is created
     */
        if (!strcasecmp(family,"ipv4"))
                addNetIfNode(ifName,AF_INET,_IFF_STATIC,ifaces);
        else if (!strcasecmp(family,"ipv6"))
                addNetIfNode(ifName,AF_INET6,_IFF_STATIC,ifaces);
        else if (!strcasecmp(family,"dual"))
                addNetIfNode(ifName,AF_DUAL,_IFF_STATIC,ifaces);
}

PRIVATE void createInterfaces(struct ifaddrs *head, NETIFACE *ifaces)
{
    /*
     * Dinamic interfaces. No interface were specify in
     * config file. Add to 'NETIFACE' list every network
     * interface available in the system
     */
        int family;
        struct ifaddrs *current;

        if (head == NULL)
                return;
        current = head;
        for (; current != NULL; current = current->ifa_next) {
                if (current->ifa_addr == NULL)
                        continue;
                family = current->ifa_addr->sa_family;
                if (current->ifa_flags & IFF_LOOPBACK)
                        continue;
                if (family == AF_INET)
                        addNetIfNode(current->ifa_name,AF_INET,_IFF_DYNAMIC,
                                     ifaces);
                else if (family == AF_INET6)
                        addNetIfNode(current->ifa_name,AF_INET6,_IFF_DYNAMIC,
                                     ifaces);
        }
}

PRIVATE void fillInterfaces(struct ifaddrs *head, NETIFACE *ifaces)
{
    /*
     * For every interface in the 'NETIFACE' list add
     * their corresponding ips (Both IPv4 & IPv6)
     */
        NETIFACE *copy;
        int index, family;
        struct ifaddrs *aux;

        copy = ifaces;
        for (; copy != NULL; copy = copy->next) {
                for (aux = head; aux != NULL; aux = aux->ifa_next) {
                        index = if_nametoindex(aux->ifa_name);
                        if (index == copy->ifIndex) {
                                if (aux->ifa_addr == NULL)
                                        continue;
                                family = aux->ifa_addr->sa_family;
                                if (family != AF_INET && family != AF_INET6)
                                        continue;
                                if (aux->ifa_flags & IFF_LOOPBACK) {
                                        copy->flags |= _IFF_LOOPBACK;
                                        continue;
                                }
                                _fillInterfaces(copy,aux);
                        }
                }
        }
}

PRIVATE void _fillInterfaces(NETIFACE *copy, struct ifaddrs *aux)
{
    /*
     * Helper function for fillInterfaces()
     */
        INADDR *ip4;
        IN6ADDR *ip6;

        switch (aux->ifa_addr->sa_family) {

        case AF_INET:
                ip4 = (INADDR *)(&((SA_IN *)aux->ifa_addr)->sin_addr);
                addNetIfIPv4(ip4,copy);
                if (!(aux->ifa_flags & IFF_RUNNING))
                        copy->flags &= ~_IFF_RUNNING;
                break;
        case AF_INET6:
                ip6 = (IN6ADDR *)(&((SA_IN6 *)aux->ifa_addr)->sin6_addr);
                addNetIfIPv6(ip6,copy);
                if (!(aux->ifa_flags & IFF_RUNNING))
                        copy->flags &= ~_IFF_RUNNING;
                break;
        }
}

PRIVATE void remLoopback(NETIFACE *ifaces)
{
    /*
     * Remove all loopback interfaces (if any) from the
     * 'NETIFACE' list
     */
        NETIFACE *current;

        if (ifaces == NULL)
                return;
        current = ifaces;
        while (current != NULL) {
                if (current->flags & _IFF_LOOPBACK) {
                        remNetIfNode(current->name,ifaces);
                        break;
                }
                current = current->next;
        }
}
