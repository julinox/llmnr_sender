/** *****************************
 * Interface to handle printing *
 * Mainly used for debugging    *
 ********************************/

#ifndef NSS_LLMNR_PRINT_H
#define NSS_LLMNR_PRINT_H
#include "nss_llmnr_defs.h"

PUBLIC void closeStream();
PUBLIC void printToStream(const char *fmt, ...);
PUBLIC void printBytes(const void *object, int size);
PUBLIC void nPrintBytes(const void *object, int size);
PUBLIC void setStream(const char *path);
#endif

