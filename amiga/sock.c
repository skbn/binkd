/*
 * sock.c -- listen socket management for AmigaOS 3
 *
 * sock.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include <exec/types.h>
#include <proto/dos.h>

#include <stdlib.h>
#include <string.h>

#include "sys.h"
#include "readcfg.h"
#include "tools.h"
#include "server.h"
#include "rfc2553.h"
#include "amiga/bsdsock.h"
#include "amiga/evloop_int.h"

extern SOCKET sockfd[];
extern int sockfd_used;
extern int server_flag;

void set_nonblock(SOCKET fd)
{
    long flag = 1L;

    if (IoctlSocket(fd, FIONBIO, (char *)&flag) != 0)
        Log(2, "IoctlSocket(FIONBIO) failed: %s", TCPERR());
}

int open_listen_sockets(BINKD_CONFIG *config)
{
    struct listenchain *ll;
    struct addrinfo hints, *ai, *head;
    int err, opt = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    sockfd_used = 0;

    for (ll = config->listen.first; ll; ll = ll->next)
    {
        err = getaddrinfo(ll->addr[0] ? ll->addr : NULL, ll->port, &hints, &head);

        if (err)
        {
            Log(1, "listen getaddrinfo(%s:%s): %s", ll->addr[0] ? ll->addr : "*", ll->port, gai_strerror(err));
            return -1;
        }

        for (ai = head; ai && sockfd_used < MAX_LISTENSOCK; ai = ai->ai_next)
        {
            SOCKET fd;
            int retries = 6;

            fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

            if (fd == INVALID_SOCKET)
            {
                Log(1, "listen socket(): %s", TCPERR());
                continue;
            }

            if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, (int)sizeof(opt)) != 0)
                Log(2, "setsockopt(SO_REUSEADDR) failed: %s", TCPERR());

            /* Bsdsocket may hold the port briefly after socket close */
            while (bind(fd, ai->ai_addr, (int)ai->ai_addrlen) != 0)
            {
                if (--retries == 0)
                {
                    Log(1, "listen bind(): %s", TCPERR());

                    soclose(fd);
                    freeaddrinfo(head);
                    return -1;
                }

                Log(2, "bind retry in 2s: %s", TCPERR());

                Delay(100UL); /* 100 ticks = 2s @ 50Hz */
            }

            if (listen(fd, 5) != 0)
            {
                Log(1, "listen(): %s", TCPERR());

                soclose(fd);
                freeaddrinfo(head);
                return -1;
            }

            set_nonblock(fd);
            sockfd[sockfd_used] = fd;
            sockfd_used++;
        }

        freeaddrinfo(head);

        Log(3, "listening on %s:%s",
            ll->addr[0] ? ll->addr : "*", ll->port);
    }

    if (sockfd_used == 0 && server_flag)
    {
        Log(1, "evloop: no listen sockets opened");
        return -1;
    }

    return 0;
}

void close_listen_sockets(void)
{
    int i;

    for (i = 0; i < sockfd_used; i++)
        soclose(sockfd[i]);

    sockfd_used = 0;
}
