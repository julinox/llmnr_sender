/** **********************************************************
 * See man 3 gethostbyname                                   *
 * Given a list of 'ANSWERS' (See nss_llmnr_answers.h) parse *
 * them, then fill 'buf' and 'result_buf                     *
 *************************************************************/

#ifndef NSS_LLMNR_S3_H
#define NSS_LLMNR_S3_H
#include "nss_llmnr_defs.h"

PUBLIC int startS3(const ARGS *args, ANSWERS *answers);
PUBLIC int parseAndBuildAnswer(const ARGS *args, U_CHAR *qrBuf);
#endif
