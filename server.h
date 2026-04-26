#ifndef _servmgr_h
#define _servmgr_h

#define MAX_LISTENSOCK 16

extern SOCKET sockfd[MAX_LISTENSOCK];
extern int sockfd_used;

#ifdef AMIGA
#include <netinet/in.h>
#endif

/*
 * Listens... Than calls protocol()
 */
void servmgr(void);

extern int ext_rand;

#endif
