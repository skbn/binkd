/*
 * bsdsock.h -- bsdsocket.library init and POSIX compat shims for AmigaOS 3
 *
 * bsdsock.h is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#ifndef _AMIGA_BSDSOCK_H
#define _AMIGA_BSDSOCK_H

#ifdef AMIGA

#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <proto/exec.h>

/* Suppress conflicting C prototypes from roadshow */
#ifndef CLIB_BSDSOCKET_PROTOS_H
#define CLIB_BSDSOCKET_PROTOS_H
#endif

/* Undefine MCLBYTES/MCLSHIFT before roadshow headers */
#ifdef MCLBYTES
#undef MCLBYTES
#endif
#ifdef MCLSHIFT
#undef MCLSHIFT
#endif

/* Roadshow SDK network headers */
#include <sys/types.h>
#include <sys/socket.h>
#include "compat_netinet_in.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <proto/socket.h> /* inline/bsdsocket.h, no clib protos */

/* Undefine conflicting unistd.h macros */
#ifdef gethostid
#undef gethostid
#endif
#ifdef getdtablesize
#undef getdtablesize
#endif
#ifdef gethostname
#undef gethostname
#endif

/* Per-task SocketBase override */
struct Library *SocketBase;

#ifdef SocketBase
#undef SocketBase
#endif
#define SocketBase SocketBase

/* Roadshow socket-specific errno values */
#include <sys/errno.h>

#define BSDSOCK_HAS_TIMEVAL 1

/* Socket-specific errno values from roadshow sys/errno.h */
#ifndef ENOTSOCK
#define ENOTSOCK 38 /* Socket operation on non-socket */
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP 45 /* Operation not supported on socket */
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED 61 /* Connection refused */
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 60 /* Connection timed out */
#endif
#ifndef ECONNRESET
#define ECONNRESET 54 /* Connection reset by peer */
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH 65 /* No route to host */
#endif

#include <errno.h>

/* sockaddr_storage fallback for roadshow */
#ifndef HAVE_SOCKADDR_STORAGE
#ifndef sockaddr_storage
struct sockaddr_storage
{
    unsigned short ss_family;
    char __ss_pad[22]; /* enough for IPv4 */
};
#endif
#define HAVE_SOCKADDR_STORAGE 1
#endif

/* Library base functions */
int amiga_sock_init();
void amiga_sock_cleanup();

/* getpid() is defined in sys.h for AMIGA */
/* amiga_sleep and sleep are defined in sys.h for AMIGA */

/* select() -> WaitSelect() wrapper with Ctrl+C handling */
#ifndef AMIGA_SELECT_DEFINED
#define AMIGA_SELECT_DEFINED

/* Forward-declare binkd_exit */
extern int binkd_exit;

/* Forward-declare Log to avoid implicit declaration warning */
extern void Log(int lev, char *s, ...);

static int amiga_select_wrap(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    ULONG sigmask = SIGBREAKF_CTRL_C;
    int rc = WaitSelect(nfds, readfds, writefds, exceptfds, timeout, &sigmask);

    /* Ctrl+C should break blocked select() loops immediately. */
    if ((sigmask & SIGBREAKF_CTRL_C) != 0)
    {
        Log(1, "Ctrl+C detected in WaitSelect, setting binkd_exit=1");
        binkd_exit = 1;
        errno = EINTR;
        return -1;
    }

    return rc;
}

#define select(n, r, w, e, t) amiga_select_wrap((n), (r), (w), (e), (t))

#endif /* AMIGA_SELECT_DEFINED */

/* FIONBIO via IoctlSocket */
#ifndef FIONBIO
#define FIONBIO 0x8004667E
#endif
#ifndef ioctl
#define ioctl(s, req, arg) IoctlSocket((s), (req), (char *)(arg))
#endif

/* inet_ntoa -> Inet_NtoA (Amiga only) */
#ifdef AMIGA
#ifdef inet_ntoa
#undef inet_ntoa
#endif
#define inet_ntoa(a) Inet_NtoA(a)
#endif

#endif /* AMIGA */
#endif /* _AMIGA_BSDSOCK_H */
