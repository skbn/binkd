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

/* Linker-compatibility global. Never used at runtime */
struct Library *SocketBase = NULL;

/* Suppress conflicting C prototypes from clib/bsdsocket_protos.h */
#ifndef CLIB_BSDSOCKET_PROTOS_H
#define CLIB_BSDSOCKET_PROTOS_H
#endif

#include <proto/socket.h>
#include <errno.h>

extern void Log(int lev, const char *s, ...);

/* _amiga_get_socket_base -- returns bsdsocket.library handle for calling task */
struct Library *_amiga_get_socket_base(void)
{
    return (struct Library *)FindTask(NULL)->tc_UserData;
}

int amiga_sock_init(void)
{
    struct Task *me = FindTask(NULL);
    struct Library *base;

    if (me->tc_UserData)
        return 0; /* already open for this task */

    base = OpenLibrary("bsdsocket.library", 0UL);

    if (!base)
    {
        fprintf(stderr, "amiga_sock_init: cannot open bsdsocket.library\n");
        return -1;
    }

    /* Store in tc_UserData and global SocketBase */
    me->tc_UserData = (APTR)base;
    SocketBase = base;

    /* Link the per-task errno to the TCP stack. */
    SetErrnoPtr(&errno, (LONG)sizeof(errno));

    return 0;
}

void amiga_sock_cleanup(void)
{
    struct Task *me = FindTask(NULL);
    struct Library *base = (struct Library *)me->tc_UserData;

    if (base)
    {
        me->tc_UserData = NULL;
        SocketBase = NULL;
        CloseLibrary(base);
    }
}

int amiga_child_sock_init(void)
{
    /* Child inherits tc_UserData = NULL, opens new handle */
    return amiga_sock_init();
}
