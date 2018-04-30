/** **********************************************
 * Send LLMNR queries and received the responses *
 * (Similar to llmnr.conflict.c)                 *
 *************************************************/

#ifndef NSS_LLMNR_S2_H
#define NSS_LLMNR_S2_H
#include "nss_llmnr_defs.h"

PUBLIC int startS2(const ARGS *args, NETIFACE *ifs, ANSWERS *answers);
#endif
