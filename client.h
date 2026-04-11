#ifndef _client_h
#define _client_h

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

/*
 * Scans queue, makes outbound ``call'', than calls protocol()
 */
void clientmgr(void *arg);

#endif
