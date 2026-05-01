/*
 * bsdsock.c -- bsdsocket.library lifecycle for AmigaOS 3
 *
 * bsdsock.c is a part of binkd project
 *
 * Copyright (C) 2026 Tanausú M. 39:190/101@amiganet
 * Licensed under the GNU GPL v2 or later
 */

#include <stdlib.h>
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

/* Global bsdsocket.library handle (no threads, single task) */
struct Library *SocketBase = NULL;

/* Suppress conflicting C prototypes from clib/bsdsocket_protos.h */
#ifndef CLIB_BSDSOCKET_PROTOS_H
#define CLIB_BSDSOCKET_PROTOS_H
#endif

#include <proto/socket.h>
#include <libraries/bsdsocket.h>
#include <errno.h>

extern void Log(int lev, const char *s, ...);

int amiga_sock_init()
{
    if (SocketBase)
        return 0; /* already open */

    SocketBase = OpenLibrary("bsdsocket.library", 0UL);

    if (!SocketBase)
    {
        fprintf(stderr, "amiga_sock_init: cannot open bsdsocket.library\n");
        return -1;
    }

    /* Link the per-task errno to the TCP stack. */
    /* Use SocketBaseTags with SBTC_ERRNOPTR (modern API, SetErrnoPtr is deprecated) */
    SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))), &errno, TAG_END);

    return 0;
}

void amiga_sock_cleanup()
{
    if (SocketBase)
    {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
    }
}
