/** *************************************************
 * Interface that groups a bunch of utils functions *
 * not directly related to any other module         *
 ****************************************************/

#ifndef NSS_LLMNR_UTILS_H
#define NSS_LLMNR_UTILS_H
#include "../include/nss_llmnr_defs.h"


PUBLIC void strToPtrStr(const void *ip, int type, char *buff);
PUBLIC char *strToDnsStr(const char *name, char *buff);
PUBLIC char *dnsStrToStr(char *name, char *buff);
PUBLIC int checkFQDN(char *name);
#endif
