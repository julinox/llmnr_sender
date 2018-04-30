/** ********************************************
 * Interface to handle a 'ANSWERS' list        *
 * This list will contain all the answers read *
 * from the actual LLMNR response packet       *
 ***********************************************/

#ifndef NSS_LLMNR_ANSWERS_H
#define NSS_LLMNR_ANSWERS_H
#include "nss_llmnr_defs.h"

typedef struct answs{
        unsigned short TYPE;
        int TTL;
        unsigned short RDLENGTH;
        void *RDATA;
        struct answs *next;
} ANSWERS;

PUBLIC ANSWERS *answersListhead();
PUBLIC void deleteAnswersList(ANSWERS **list);
PUBLIC void printAnswers(ANSWERS *list);
PUBLIC int newAnswer(ANSWERS *list, U_SHORT type, int ttl, void *data,
                     U_SHORT dLen);
PUBLIC int countAnswers(ANSWERS *list);
PUBLIC int countAnswersByType(U_SHORT type, ANSWERS *list);
PUBLIC int countAnswersBytes(U_SHORT type, ANSWERS *list);
PUBLIC int getAnswerTTL(ANSWERS *list);
PUBLIC int isValidAnswer(U_SHORT type, ANSWERS *list);
#endif
