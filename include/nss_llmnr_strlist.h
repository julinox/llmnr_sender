/** **************************************************
 * Linked list interface to handle a list of strings *
 *****************************************************/

#ifndef NSS_LLMNR_STRLIST_H
#define NSS_LLMNR_STRLIST_H
#include "nss_llmnr_defs.h"

typedef struct stringNode {
        char *string;
        struct stringNode *next;
} STRNODE;

PUBLIC STRNODE *strNodeHead();
PUBLIC STRNODE *split(char *str, char delimiter);
PUBLIC void printStrNodes(STRNODE *strList);
PUBLIC void deleteStrNodeList(STRNODE **strList);
PUBLIC char *getNodeAt(STRNODE *strList, int position);
PUBLIC int countStrNodes(STRNODE *strList);
PUBLIC int newStrNode(char *string, STRNODE *strList);
#endif
