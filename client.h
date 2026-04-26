#ifndef _client_h
#define _client_h

#ifdef AMIGA
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

/*
 * Scans queue, makes outbound ``call'', than calls protocol()
 */
void clientmgr(void *arg);

#ifdef AMIGA
/* Direct outbound call for evloop.c (no-ixemul, no-threads build) */
int call0(FTN_NODE *node, BINKD_CONFIG *config);
#endif

#endif
