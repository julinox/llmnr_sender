/** *****************************************
 * A bunch of common macros used accros the *
 * 'nss_llmnr' module                       *
 ********************************************/

#ifndef NSS_LLMNR_DEFS_H
#define NSS_LLMNR_DEFS_H

#define PUBLIC
#define PRIVATE static
#define U_CHAR unsigned char
#define U_SHORT unsigned short
#define U_INT unsigned int

#define TRUE 1
#define FALSE 0
#define SUCCESS 0
#define FAILURE -1
#define SYSFAILURE 1
#define MAXIFACES 16
#define HEADSZ 12
#define IPV4LEN 4
#define IPV6LEN 16
#define HOSTNAMEMAX 255
#define RRTTL 30
#define CHEADZ 20
#define SNDBUFSZ 512
#define RCVBUFSZ 512
#define ANCBUFSZ 256
#define TCPBUFSZ 2048
#define LLMNRPORT 5355
#define LLMNR_TIMEOUT 150
#define JITTER_INTERVAL 100

#define _STATUS enum nss_status
#define _HOST struct hostent
#define INADDR struct in_addr
#define IN6ADDR struct in6_addr
#define SA struct sockaddr
#define SA_IN struct sockaddr_in
#define SA_IN6 struct sockaddr_in6
#define SA_STORAGE struct sockaddr_storage

enum RRTYPE {
        NONE = 0,
        A = 1,
        NS = 2,
        CNAME = 5,
        SOA = 6,
        PTR = 12,
        HINFO = 13,
        MX = 15,
        TXT = 16,
        AAAA = 28,
        SRV = 33,
        ANY = 255
};

typedef struct {
        char *buf;
        const char *name;
        const char *cName;
        int *errnop;
        int *herrnop;
        U_SHORT type;
        size_t bufLen;
        _HOST *resultBuf;
        const void *addr;
        size_t len;
        int format;
} ARGS;
#endif
